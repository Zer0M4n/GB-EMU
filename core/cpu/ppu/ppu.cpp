// ============================================================
// PPU.CPP - CORREGIDO: Window layer + bounds check + timing fix
// ============================================================

#include "ppu.h"
#include <algorithm>
#include <iostream>
#include <iomanip>

ppu::ppu(mmu& mmu_ref) : memory(mmu_ref), debug_enabled(false), last_line_logged(0xFF), last_mode_logged(0xFF)
{
    dots_counter  = 0;
    current_line  = 0;
    current_mode  = 2;
    scanline_dots = 0;
    frame_complete   = false;
    prev_stat_line   = false;
    vblank_irq_fired = false;
    window_line_counter = 0;   // ← NUEVO: contador interno de la Window

    gfx.resize(160 * 144);

    // Paleta clásica DMG – formato 0xAABBGGRR (little-endian para ImageData)
    palette[0] = 0xFF0FBC9B; // Verde claro
    palette[1] = 0xFF0FAC8B; // Verde medio
    palette[2] = 0xFF306230; // Verde oscuro
    palette[3] = 0xFF0F380F; // Verde muy oscuro

    std::fill(gfx.begin(), gfx.end(), palette[0]);

    memory.writeMemory(0xFF44, 0);
    memory.writeMemory(0xFF41, 0x82);

    std::cout << "[PPU INIT] Listo. Window layer habilitada.\n";
}

// ============================================================
// DEBUG STUBS (comentados para producción)
// ============================================================
void ppu::enable_debug(bool enable)              { (void)enable; }
void ppu::debug_scanline_report(const char*)     {}
void ppu::debug_mode_change(uint8_t, uint8_t)    {}

// ============================================================
// STEP
// ============================================================
void ppu::step(int cpu_cycles)
{
    for (int i = 0; i < cpu_cycles; i++)
        step_one_dot();
}

// ============================================================
// STEP ONE DOT
// ============================================================
void ppu::step_one_dot()
{
    uint8_t lcdc = memory.IO[0x40];

    // LCD apagado → resetear estado
    if (!(lcdc & 0x80))
    {
        scanline_dots    = 0;
        current_line     = 0;
        current_mode     = 0;
        window_line_counter = 0;
        memory.IO[0x44]  = 0;
        memory.IO[0x41]  = (memory.IO[0x41] & 0xFC) | 0x80;
        vblank_irq_fired = false;
        prev_stat_line   = false;
        return;
    }

    dots_counter++;
    scanline_dots++;

    // ── State Machine de modos ──────────────────────────────
    if (current_line < 144)
    {
        if (scanline_dots <= 80)
        {
            if (current_mode != 2)
            {
                current_mode = 2;
                memory.IO[0x41] = (memory.IO[0x41] & 0xFC) | 2;
                update_stat_interrupt();
            }
        }
        else if (scanline_dots <= 252)
        {
            if (current_mode != 3)
            {
                current_mode = 3;
                memory.IO[0x41] = (memory.IO[0x41] & 0xFC) | 3;
                // Modo 3 no genera STAT IRQ
            }
        }
        else
        {
            if (current_mode != 0)
            {
                current_mode = 0;
                memory.IO[0x41] = (memory.IO[0x41] & 0xFC) | 0;
                draw_scanline();          // Renderizar al entrar a HBlank
                update_stat_interrupt();
            }
        }
    }
    else
    {
        // VBlank: líneas 144-153
        if (current_mode != 1)
        {
            current_mode = 1;
            memory.IO[0x41] = (memory.IO[0x41] & 0xFC) | 1;

            if (!vblank_irq_fired)
            {
                memory.IO[0x0F] |= 0x01;   // IF bit 0
                vblank_irq_fired = true;
                frame_complete   = true;

                static int vblank_count = 0;
                vblank_count++;
                if (vblank_count <= 10 || vblank_count % 60 == 0)
                    std::cout << "[PPU] VBlank #" << vblank_count
                              << " fired! IF=0x" << std::hex << (int)memory.IO[0x0F]
                              << " LY=" << std::dec << (int)current_line << "\n";
            }
            update_stat_interrupt();
        }
    }

    // ── Fin de scanline (456 dots) ──────────────────────────
    if (scanline_dots >= 456)
    {
        scanline_dots -= 456;
        current_line++;
        memory.IO[0x44] = current_line;

        check_lyc_coincidence();

        if (current_line > 153)
        {
            current_line        = 0;
            memory.IO[0x44]     = 0;
            vblank_irq_fired    = false;
            window_line_counter = 0;   // ← Resetear contador de Window al inicio de frame
        }
    }
}

// ============================================================
// CHECK LYC COINCIDENCE
// ============================================================
void ppu::check_lyc_coincidence()
{
    uint8_t lyc  = memory.IO[0x45];
    uint8_t stat = memory.IO[0x41];

    if (current_line == lyc)
    {
        if (!(stat & 0x04))
        {
            memory.IO[0x41] = stat | 0x04;
            update_stat_interrupt();
        }
    }
    else
    {
        if (stat & 0x04)
        {
            memory.IO[0x41] = stat & ~0x04;
            update_stat_interrupt();
        }
    }
}

// ============================================================
// UPDATE STAT INTERRUPT (Wired-OR, edge-triggered)
// ============================================================
void ppu::update_stat_interrupt()
{
    uint8_t stat = memory.IO[0x41];
    bool line_state = false;

    if ((stat & (1 << 3)) && current_mode == 0) line_state = true;
    if ((stat & (1 << 4)) && current_mode == 1) line_state = true;
    if ((stat & (1 << 5)) && current_mode == 2) line_state = true;
    if ((stat & (1 << 6)) && (stat & (1 << 2)))  line_state = true;

    if (line_state && !prev_stat_line)
        memory.IO[0x0F] |= 0x02;   // IF bit 1 (STAT)

    prev_stat_line = line_state;
}

// ============================================================
// DRAW SCANLINE
// ============================================================
void ppu::draw_scanline()
{
    uint8_t lcdc = memory.IO[0x40];
    uint8_t ly   = current_line;

    if (!(lcdc & 0x80)) {
        for (int x = 0; x < 160; x++) {
            gfx[ly * 160 + x] = palette[0];
            bg_priority[x]    = 0;
        }
        return;
    }

    // Inicializar prioridad de BG a 0
    for (int x = 0; x < 160; x++) {
        bg_priority[x] = 0;
        gfx[ly * 160 + x] = palette[0];
    }

    // Bit 0 LCDC: BG + Window habilitados (en DMG, controla ambos)
    if (lcdc & 0x01)
    {
        draw_background();
        draw_window();     // ← ANTES solo era un TODO
    }

    // Bit 1 LCDC: Sprites habilitados
    if (lcdc & 0x02)
        draw_sprites();
}

// ============================================================
// DRAW BACKGROUND
// ============================================================
void ppu::draw_background()
{
    uint8_t lcdc = memory.IO[0x40];
    uint8_t scy  = memory.IO[0x42];
    uint8_t scx  = memory.IO[0x43];
    uint8_t ly   = current_line;
    uint8_t bgp  = memory.IO[0x47];

    // Mapa de tiles: bit 3 de LCDC → 0x9C00 : 0x9800
    uint16_t tile_map_base       = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    bool     signed_tile_addr    = !(lcdc & 0x10);

    uint8_t  y_pos    = scy + ly;
    uint16_t tile_row = (y_pos / 8) * 32;

    for (int x = 0; x < 160; x++)
    {
        uint8_t  x_pos    = scx + (uint8_t)x;
        uint16_t tile_col = x_pos / 8;
        uint16_t map_addr = tile_map_base + tile_row + tile_col;

        // Leer tile ID de VRAM
        uint8_t tile_id = memory.VRAM[map_addr - 0x8000];

        // Calcular dirección de datos del tile
        uint16_t tile_data_base;
        if (signed_tile_addr)
            tile_data_base = (uint16_t)(0x9000 + (int8_t)tile_id * 16);
        else
            tile_data_base = 0x8000 + (uint16_t)tile_id * 16;

        // Línea dentro del tile (0-7), 2 bytes por línea
        uint8_t tile_line = (y_pos % 8) * 2;

        // Bounds check VRAM (8KB = 0x2000 bytes)
        uint32_t vram_offset = (uint32_t)(tile_data_base - 0x8000) + tile_line;
        if (vram_offset + 1 >= memory.VRAM.size()) {
            gfx[ly * 160 + x] = palette[0];
            bg_priority[x]    = 0;
            continue;
        }

        uint8_t lo = memory.VRAM[vram_offset];
        uint8_t hi = memory.VRAM[vram_offset + 1];

        int color_bit = 7 - (x_pos % 8);
        int color_num = (((hi >> color_bit) & 1) << 1) | ((lo >> color_bit) & 1);
        int color     = (bgp >> (color_num * 2)) & 0x03;

        gfx[ly * 160 + x] = palette[color];
        bg_priority[x]     = color_num;
    }
}

// ============================================================
// DRAW WINDOW  ← IMPLEMENTACIÓN NUEVA
// ============================================================
void ppu::draw_window()
{
    uint8_t lcdc = memory.IO[0x40];

    // Bit 5 de LCDC: Window habilitada
    if (!(lcdc & 0x20)) return;

    uint8_t wy  = memory.IO[0x4A]; // Window Y position
    uint8_t wx  = memory.IO[0x4B]; // Window X position - 7
    uint8_t ly  = current_line;
    uint8_t bgp = memory.IO[0x47];

    // La Window solo se dibuja si LY >= WY
    if (ly < wy) return;

    // WX < 7 es comportamiento especial / fuera de pantalla
    // WX=7 significa que la Window empieza en X=0
    if (wx > 166) return;  // Completamente fuera de pantalla

    // Mapa de tiles de la Window: bit 6 de LCDC → 0x9C00 : 0x9800
    uint16_t tile_map_base    = (lcdc & 0x40) ? 0x9C00 : 0x9800;
    bool     signed_tile_addr = !(lcdc & 0x10);  // Mismo bit 4 que BG

    // La Window usa su propio contador de líneas interno (no SCY)
    // window_line_counter se incrementa cada vez que se dibuja la Window en una línea
    uint16_t tile_row = (window_line_counter / 8) * 32;
    uint8_t  tile_y   = window_line_counter % 8;

    // Pixel X inicial en pantalla (WX - 7, mínimo 0)
    int screen_x_start = (int)wx - 7;
    if (screen_x_start < 0) screen_x_start = 0;

    bool window_drawn_this_line = false;

    for (int screen_x = screen_x_start; screen_x < 160; screen_x++)
    {
        // Posición dentro de la Window
        int win_x = screen_x - screen_x_start;

        uint16_t tile_col  = win_x / 8;
        uint16_t map_addr  = tile_map_base + tile_row + tile_col;

        uint8_t tile_id = memory.VRAM[map_addr - 0x8000];

        // Dirección de datos del tile (mismo método que BG)
        uint16_t tile_data_base;
        if (signed_tile_addr)
            tile_data_base = (uint16_t)(0x9000 + (int8_t)tile_id * 16);
        else
            tile_data_base = 0x8000 + (uint16_t)tile_id * 16;

        uint32_t vram_offset = (uint32_t)(tile_data_base - 0x8000) + tile_y * 2;
        if (vram_offset + 1 >= memory.VRAM.size()) continue;

        uint8_t lo = memory.VRAM[vram_offset];
        uint8_t hi = memory.VRAM[vram_offset + 1];

        int color_bit = 7 - (win_x % 8);
        int color_num = (((hi >> color_bit) & 1) << 1) | ((lo >> color_bit) & 1);
        int color     = (bgp >> (color_num * 2)) & 0x03;

        gfx[ly * 160 + screen_x] = palette[color];
        bg_priority[screen_x]    = color_num;  // Para prioridad de sprites sobre Window

        window_drawn_this_line = true;
    }

    // Incrementar contador interno de la Window solo si se dibujó algo
    if (window_drawn_this_line)
        window_line_counter++;
}

// ============================================================
// DRAW SPRITES
// ============================================================
void ppu::draw_sprites()
{
    uint8_t lcdc = memory.IO[0x40];
    uint8_t ly   = current_line;

    int     sprite_height = (lcdc & 0x04) ? 16 : 8;
    uint8_t obp0          = memory.IO[0x48];
    uint8_t obp1          = memory.IO[0x49];

    struct SpriteEntry { int x, y, tile, flags, oam_index; };
    SpriteEntry line_sprites[10];
    int sprites_on_line = 0;

    for (int i = 0; i < 40 && sprites_on_line < 10; i++)
    {
        int oam_addr = i * 4;
        int y_pos    = (int)memory.OAM[oam_addr]     - 16;
        int x_pos    = (int)memory.OAM[oam_addr + 1] - 8;
        int tile_num = memory.OAM[oam_addr + 2];
        int flags    = memory.OAM[oam_addr + 3];

        if (ly >= y_pos && ly < y_pos + sprite_height)
        {
            line_sprites[sprites_on_line++] = { x_pos, y_pos, tile_num, flags, i };
        }
    }

    // Ordenar por X (menor X = mayor prioridad en DMG)
    for (int i = 0; i < sprites_on_line - 1; i++)
        for (int j = i + 1; j < sprites_on_line; j++)
            if (line_sprites[j].x < line_sprites[i].x)
                std::swap(line_sprites[i], line_sprites[j]);

    // Renderizar en orden inverso (mayor prioridad encima)
    for (int s = sprites_on_line - 1; s >= 0; s--)
    {
        int x_pos    = line_sprites[s].x;
        int y_pos    = line_sprites[s].y;
        int tile_num = line_sprites[s].tile;
        int flags    = line_sprites[s].flags;

        bool priority  = (flags & 0x80) != 0;
        bool y_flip    = (flags & 0x40) != 0;
        bool x_flip    = (flags & 0x20) != 0;
        bool use_obp1  = (flags & 0x10) != 0;

        if (sprite_height == 16) tile_num &= 0xFE;

        int tile_y = (int)ly - y_pos;
        if (y_flip) tile_y = (sprite_height - 1) - tile_y;

        int actual_tile = tile_num;
        if (sprite_height == 16 && tile_y >= 8) {
            actual_tile = tile_num + 1;
            tile_y -= 8;
        }

        // Sprites siempre usan 0x8000 (unsigned)
        uint32_t tile_addr = (uint32_t)actual_tile * 16 + (uint32_t)tile_y * 2;
        if (tile_addr + 1 >= memory.VRAM.size()) continue;

        uint8_t lo = memory.VRAM[tile_addr];
        uint8_t hi = memory.VRAM[tile_addr + 1];

        uint8_t pal_data = use_obp1 ? obp1 : obp0;

        for (int px = 0; px < 8; px++)
        {
            int screen_x = x_pos + px;
            if (screen_x < 0 || screen_x >= 160) continue;

            int color_bit = x_flip ? px : (7 - px);
            int color_num = (((hi >> color_bit) & 1) << 1) | ((lo >> color_bit) & 1);

            if (color_num == 0) continue;  // Transparente

            // Prioridad: si el sprite está detrás del BG y el BG no es color 0
            if (priority && bg_priority[screen_x] != 0) continue;

            int color = (pal_data >> (color_num * 2)) & 0x03;
            gfx[ly * 160 + screen_x] = palette[color];
        }
    }
}

// ============================================================
// CAN ACCESS VRAM / OAM
// ============================================================
bool ppu::can_access_vram() const
{
    if (!(memory.readMemory(0xFF40) & 0x80)) return true;
    return current_mode != 3;
}

bool ppu::can_access_oam() const
{
    if (!(memory.readMemory(0xFF40) & 0x80)) return true;
    return (current_mode == 0 || current_mode == 1);
}