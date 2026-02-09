// ============================================================
// PPU.CPP - CON DEBUGGER INCRUSTADO PARA MONITOREAR SCANLINE
// ============================================================

#include "ppu.h"
#include <algorithm>
#include <iostream>
#include <iomanip>

ppu::ppu(mmu& mmu_ref) : memory(mmu_ref), debug_enabled(false), last_line_logged(0xFF), last_mode_logged(0xFF)
{
    dots_counter = 0;
    current_line = 0;
    current_mode = 2;      // Iniciar en modo 2 (OAM Scan) - estado correcto al boot
    scanline_dots = 0;
    frame_complete = false;
    prev_stat_line = false;
    vblank_irq_fired = false;
    
    // Inicializar buffer gráfico (160 * 144 píxeles)
    gfx.resize(160 * 144);
    
    // Paleta clásica "Game Boy" - Formato ABGR para little-endian + JavaScript ImageData
    // ImageData espera bytes [R, G, B, A] en memoria
    // En little-endian, uint32_t se almacena LSB first
    // Así que usamos 0xAABBGGRR para que en memoria sea [RR, GG, BB, AA]
    //
    // Colores clásicos del Game Boy DMG (tonos verdes):
    palette[0] = 0xFF0FBC9B; // Color 0: RGB(155, 188, 15) - Verde claro
    palette[1] = 0xFF0FAC8B; // Color 1: RGB(139, 172, 15) - Verde medio
    palette[2] = 0xFF306230; // Color 2: RGB(48, 98, 48)   - Verde oscuro
    palette[3] = 0xFF0F380F; // Color 3: RGB(15, 56, 15)   - Verde muy oscuro
    
    // Inicializar buffer con color 0 (verde claro)
    std::fill(gfx.begin(), gfx.end(), palette[0]);
    
    std::cout << "[PPU INIT] Buffer inicializado. Paleta: "
              << "0=0x" << std::hex << palette[0]
              << " 1=0x" << palette[1]
              << " 2=0x" << palette[2]
              << " 3=0x" << palette[3] << std::dec << "\n";
    
    // Inicializar registros
    memory.writeMemory(0xFF44, 0); // LY = 0
    memory.writeMemory(0xFF41, 0x82); // STAT: bit 7 siempre 1, modo 2
}

// ============================================================
// ENABLE DEBUG - ACTIVAR/DESACTIVAR DEBUGGER
// ============================================================
void ppu::enable_debug(bool enable) {
    // debug_enabled = enable;
    // if (debug_enabled) {
    //     std::cout << "[PPU DEBUG] Debugger activado\n";
    // } else {
    //     std::cout << "[PPU DEBUG] Debugger desactivado\n";
    // }
}

// ============================================================
// DEBUG SCANLINE REPORT - REPORTE DETALLADO DE LA SCANLINE
// ============================================================
void ppu::debug_scanline_report(const char* event) {
    /*if (!debug_enabled) return;
    
    uint8_t lcdc = memory.readMemory(0xFF40);
    uint8_t stat = memory.readMemory(0xFF41);
    uint8_t ly = memory.readMemory(0xFF44);
    uint8_t lyc = memory.readMemory(0xFF45);
    uint8_t scy = memory.readMemory(0xFF42);
    uint8_t scx = memory.readMemory(0xFF43);
    uint8_t bgp = memory.readMemory(0xFF47);
    uint8_t if_reg = memory.readMemory(0xFF0F);
    
    // Mostrar información cada vez que cambia la línea o hay evento
    if (event || ly != last_line_logged) {
        std::cout << "\n[PPU DEBUG] ";
        
        // Evento si hay
        if (event) std::cout << event << " | ";
        
        // Información de la scanline
        std::cout << "LY=" << std::dec << (int)ly << "/" << (int)lyc
                  << " | Dots=" << scanline_dots << "/456"
                  << " | Mode=" << (int)current_mode;
        
        // Nombre del modo
        const char* mode_names[] = {"HBlank", "VBlank", "OAM Scan", "Drawing"};
        std::cout << "(" << mode_names[current_mode] << ")";
        
        // Información de registros
        std::cout << " | LCDC=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)lcdc
                  << " | STAT=0x" << std::setw(2) << (int)stat
                  << " | IF=0x" << std::setw(2) << (int)if_reg
                  << std::dec << "\n";
        
        // Información de posición de cámara
        std::cout << "         SCY=" << (int)scy << " | SCX=" << (int)scx 
                  << " | BGP=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)bgp << "\n";
        
        last_line_logged = ly;
    }*/
}

// ============================================================
// DEBUG MODE CHANGE - REPORTE CUANDO CAMBIA DE MODO
// ============================================================
void ppu::debug_mode_change(uint8_t old_mode, uint8_t new_mode) {
    // if (!debug_enabled || old_mode == new_mode) return;
    
    // const char* mode_names[] = {"HBlank", "VBlank", "OAM Scan", "Drawing"};
    // uint8_t ly = memory.readMemory(0xFF44);
    
    // std::cout << "[PPU DEBUG] Cambio de modo en LY=" << std::dec << (int)ly 
    //           << ": " << mode_names[old_mode] << " → " << mode_names[new_mode]
    //           << " (dot " << scanline_dots << ")\n";
}

// ============================================================
// STEP - AVANZA LA PPU POR T-CYCLES
// ============================================================
void ppu::step(int cpu_cycles) {
    // El parámetro es T-cycles. En Game Boy:
    // 1 T-cycle = 1 dot de PPU (relación 1:1)
    int dots_to_advance = cpu_cycles;
    
    // Avanzar dot por dot para máxima precisión
    for (int i = 0; i < dots_to_advance; i++) {
        step_one_dot();
    }
}

// ============================================================
// STEP ONE DOT - AVANZA UN DOT DE PPU (WIRED-OR STAT FIX)
// ============================================================
void ppu::step_one_dot() {
    // 1. Verificar LCD habilitado (bit 7 de LCDC)
    uint8_t lcdc = memory.readMemory(0xFF40);
    if (!(lcdc & 0x80)) {
        // LCD apagado: Resetear todo
        scanline_dots = 0;
        current_line = 0;
        current_mode = 0;
        memory.IO[0x44] = 0;
        memory.IO[0x41] = (memory.IO[0x41] & 0xFC) | 0x80; // Mantener bits, modo 0
        vblank_irq_fired = false;
        prev_stat_line = false;
        return;
    }

    // 2. Avanzar tiempo
    dots_counter++;
    scanline_dots++;
    
    // Guardar modo anterior para detectar transiciones
    uint8_t old_mode = current_mode;

    // 3. Evaluar Transiciones de Modo (State Machine)
    if (current_line < 144) {
        // --- LÍNEAS VISIBLES (0-143) ---
        if (scanline_dots <= 80) {
            // Modo 2: OAM Scan (80 dots)
            if (current_mode != 2) {
                current_mode = 2;
                // Actualizar registro STAT (bits 0-1)
                uint8_t stat = memory.IO[0x41];
                stat = (stat & 0xFC) | 2;
                memory.IO[0x41] = stat;
                // Verificar interrupción STAT
                update_stat_interrupt();
            }
        } else if (scanline_dots <= 252) {
            // Modo 3: Drawing (~172 dots, variable)
            if (current_mode != 3) {
                current_mode = 3;
                uint8_t stat = memory.IO[0x41];
                stat = (stat & 0xFC) | 3;
                memory.IO[0x41] = stat;
                // Modo 3 no tiene interrupción STAT asociada
            }
        } else {
            // Modo 0: HBlank (resto hasta 456 dots)
            if (current_mode != 0) {
                current_mode = 0;
                uint8_t stat = memory.IO[0x41];
                stat = (stat & 0xFC) | 0;
                memory.IO[0x41] = stat;
                // Renderizar línea al entrar a HBlank
                draw_scanline();
                // Verificar interrupción STAT (modo 0 puede disparar)
                update_stat_interrupt();
            }
        }
    } else {
        // --- VBLANK (Líneas 144-153) ---
        if (current_mode != 1) {
            current_mode = 1;
            uint8_t stat = memory.IO[0x41];
            stat = (stat & 0xFC) | 1;
            memory.IO[0x41] = stat;
            
            // VBlank IRQ (IF bit 0) - Solo una vez al entrar
            if (!vblank_irq_fired) {
                memory.IO[0x0F] |= 0x01;
                vblank_irq_fired = true;
                frame_complete = true;
                
                // DEBUG: Log cada VBlank
                static int vblank_count = 0;
                vblank_count++;
                if (vblank_count <= 10 || vblank_count % 60 == 0) {
                    std::cout << "[PPU] VBlank #" << vblank_count 
                              << " fired! IF=0x" << std::hex << (int)memory.IO[0x0F]
                              << " LY=" << std::dec << (int)current_line
                              << "\n";
                }
            }
            
            // STAT IRQ para modo 1 (si está habilitado)
            update_stat_interrupt();
        }
    }

    // 4. Fin de Scanline (456 dots)
    if (scanline_dots >= 456) {
        scanline_dots -= 456;
        current_line++;
        memory.IO[0x44] = current_line;

        // Verificar coincidencia LY=LYC al cambiar de línea
        check_lyc_coincidence();

        // 5. Fin de Frame (Línea 154 = reset a línea 0)
        if (current_line > 153) {
            current_line = 0;
            memory.IO[0x44] = 0;
            vblank_irq_fired = false;
            // El modo cambiará a 2 en el próximo step naturalmente
        }
    }
}

// ============================================================
// CHECK LYC COINCIDENCE - VERIFICAR COINCIDENCIA LY == LYC
// ============================================================
void ppu::check_lyc_coincidence() {
    uint8_t lyc = memory.IO[0x45];  // LYC register
    uint8_t stat = memory.IO[0x41]; // STAT register
    
    if (current_line == lyc) {
        // Coincidencia: Encendemos el Bit 2 (LYC Flag)
        if (!(stat & 0x04)) {
            stat |= 0x04;
            memory.IO[0x41] = stat;
            // Al cambiar el bit, el estado de la línea puede cambiar
            update_stat_interrupt();
        }
    } else {
        // No coincidencia: Apagamos el Bit 2
        if (stat & 0x04) {
            stat &= ~0x04;
            memory.IO[0x41] = stat;
            // También verificar si cambia el estado de interrupción
            update_stat_interrupt();
        }
    }
}

// ============================================================
// UPDATE STAT INTERRUPT - WIRED-OR LOGIC (EDGE-TRIGGERED)
// ============================================================
void ppu::update_stat_interrupt() {
    uint8_t stat = memory.IO[0x41];
    bool current_line_state = false;

    // 1. Revisar si alguna fuente habilitada está activa (Wired-OR)
    
    // Bit 3: Habilitar IRQ para Modo 0 (H-Blank)
    if ((stat & (1 << 3)) && (current_mode == 0)) {
        current_line_state = true;
    }

    // Bit 4: Habilitar IRQ para Modo 1 (V-Blank)
    if ((stat & (1 << 4)) && (current_mode == 1)) {
        current_line_state = true;
    }

    // Bit 5: Habilitar IRQ para Modo 2 (OAM)
    if ((stat & (1 << 5)) && (current_mode == 2)) {
        current_line_state = true;
    }

    // Bit 6: Habilitar IRQ para LYC=LY
    // El bit 2 es el flag de coincidencia que se actualiza en check_lyc
    if ((stat & (1 << 6)) && (stat & (1 << 2))) {
        current_line_state = true;
    }

    // 2. DETECCIÓN DE FLANCO (Rising Edge)
    // Solo disparamos si antes estaba BAJO y ahora es ALTO
    if (current_line_state && !prev_stat_line) {
        // Solicitar Interrupción STAT (Bit 1 del registro IF 0xFF0F)
        memory.IO[0x0F] |= 0x02;
    }

    // 3. Guardar estado para el siguiente ciclo (El "Blocking" ocurre aquí)
    prev_stat_line = current_line_state;
}

// ============================================================
// DRAW SCANLINE - RENDERIZA UNA LÍNEA DE ESCANEO
// ============================================================
void ppu::draw_scanline() {
    uint8_t lcdc = memory.IO[0x40];  // Leer directo del IO para evitar overhead
    uint8_t ly = current_line;
    
    // DEBUG: Log primeras scanlines para confirmar que se está renderizando
    static int scanline_render_count = 0;
    scanline_render_count++;
    if (scanline_render_count <= 5 || scanline_render_count == 144) {
        std::cout << "[PPU RENDER] Scanline " << std::dec << (int)ly 
                  << " LCDC=0x" << std::hex << (int)lcdc
                  << " BG_EN=" << ((lcdc & 0x01) ? "YES" : "NO")
                  << " LCD_EN=" << ((lcdc & 0x80) ? "YES" : "NO")
                  << std::dec << "\n";
    }
    
    // Si LCD está apagado (bit 7), no renderizar
    if (!(lcdc & 0x80)) {
        // Llenar de blanco cuando LCD está apagado
        for (int x = 0; x < 160; x++) {
            gfx[ly * 160 + x] = palette[0];
            bg_priority[x] = 0;
        }
        return;
    }
    
    // Verificar si el fondo está activado (Bit 0 LCDC)
    if (lcdc & 0x01) {
        draw_background();
    } else {
        // Sin fondo, llenar de blanco y marcar prioridad 0
        for (int x = 0; x < 160; x++) {
            gfx[ly * 160 + x] = palette[0];
            bg_priority[x] = 0;  // BG color 0 = sprites tienen prioridad
        }
    }
    
    // Renderizar sprites si están habilitados (Bit 1 LCDC)
    if (lcdc & 0x02) {
        draw_sprites();
    }
    
    // DEBUG: Análisis del buffer después de cada frame
    static int buffer_check_frame = 0;
    if (ly == 143) {
        buffer_check_frame++;
        if (buffer_check_frame == 60 || buffer_check_frame == 120 || buffer_check_frame == 180) {
            // Contar colores únicos en el buffer
            int color_counts[4] = {0, 0, 0, 0};
            for (int i = 0; i < 160 * 144; i++) {
                for (int c = 0; c < 4; c++) {
                    if (gfx[i] == palette[c]) {
                        color_counts[c]++;
                        break;
                    }
                }
            }
            std::cout << "\n[BUFFER ANALYSIS] Frame " << buffer_check_frame << ":\n"
                      << "  Color 0 (lightest): " << color_counts[0] << " pixels\n"
                      << "  Color 1: " << color_counts[1] << " pixels\n"
                      << "  Color 2: " << color_counts[2] << " pixels\n"
                      << "  Color 3 (darkest): " << color_counts[3] << " pixels\n"
                      << "  LCDC=0x" << std::hex << (int)lcdc << std::dec << "\n\n";
        }
    }
    
    // TODO: Añadir renderizado de ventana (Bit 5 LCDC)
}

// ============================================================
// DRAW BACKGROUND - RENDERIZA EL FONDO
// ============================================================
void ppu::draw_background() {
    uint8_t lcdc = memory.IO[0x40];
    uint8_t scy = memory.IO[0x42];
    uint8_t scx = memory.IO[0x43];
    uint8_t ly = current_line;
    uint8_t bgp = memory.IO[0x47]; // Paleta Fondo

    // DEBUG: Log primera línea para verificar configuración
    static bool logged_first = false;
    static int non_zero_pixels = 0;
    static int total_pixels_checked = 0;
    
    if (!logged_first && ly == 0) {
        logged_first = true;
        
        // Mapa de Tiles: ¿0x9C00 o 0x9800? (Bit 3)
        uint16_t tile_map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
        bool signed_addr = !(lcdc & 0x10);
        
        std::cout << "[PPU BG] First render config:\n"
                  << "  SCX=" << std::dec << (int)scx << " SCY=" << (int)scy << "\n"
                  << "  BGP=0x" << std::hex << (int)bgp << " LCDC=0x" << (int)lcdc << "\n"
                  << "  TileMap=0x" << tile_map_base << " TileData=" 
                  << (signed_addr ? "0x8800 (signed)" : "0x8000 (unsigned)") << "\n";
        
        // Sample tile map entries
        std::cout << "  First 8 tile IDs in map: ";
        for (int i = 0; i < 8; i++) {
            std::cout << std::hex << (int)memory.VRAM[tile_map_base - 0x8000 + i] << " ";
        }
        std::cout << "\n";
        
        // Sample tile data
        uint8_t first_tile = memory.VRAM[tile_map_base - 0x8000];
        uint16_t tile_data_addr = signed_addr ? 
            (0x9000 + ((int8_t)first_tile * 16)) : 
            (0x8000 + (first_tile * 16));
        std::cout << "  First tile (#" << (int)first_tile << ") data at 0x" << tile_data_addr << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)memory.VRAM[tile_data_addr - 0x8000 + i] << " ";
        }
        std::cout << std::dec << "\n";
    }

    // Mapa de Tiles: ¿0x9C00 o 0x9800? (Bit 3)
    uint16_t tile_map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;

    // Datos de Tiles: ¿0x8000 o 0x8800? (Bit 4)
    bool signed_tile_addressing = !(lcdc & 0x10);

    uint8_t y_pos = scy + ly;
    uint16_t tile_row = (y_pos / 8) * 32;

    for (int x = 0; x < 160; x++) {
        uint8_t x_pos = x + scx;
        
        // Qué tile es (columna)
        uint16_t tile_col = (x_pos / 8);
        uint16_t tile_addr = tile_map_base + tile_row + tile_col;
        
        // Índice del tile - Leer de VRAM directamente
        uint8_t tile_num = memory.VRAM[tile_addr - 0x8000];
        
        // CALCULAR DIRECCIÓN DEL TILE DATA
        uint16_t tile_location = 0;
        if (signed_tile_addressing) {
            // Modo 0x8800: 0x9000 base + índice con signo (-128 a 127)
            tile_location = 0x9000 + ((int8_t)tile_num * 16);
        } else {
            // Modo 0x8000: 0x8000 base + índice sin signo (0 a 255)
            tile_location = 0x8000 + (tile_num * 16);
        }

        // Qué línea vertical del tile (0-7) * 2 bytes
        uint8_t line = (y_pos % 8) * 2;
        
        // Leer datos del tile de VRAM
        uint8_t data1 = memory.VRAM[tile_location - 0x8000 + line];
        uint8_t data2 = memory.VRAM[tile_location - 0x8000 + line + 1];

        // Bit del color (invertido, bit 7 es pixel 0)
        int color_bit = 7 - (x_pos % 8);
        
        // Combinar bits para obtener color 0-3
        int color_num = ((data2 >> color_bit) & 1) << 1;
        color_num |= ((data1 >> color_bit) & 1);

        // Mapear con paleta (BGP)
        int color = (bgp >> (color_num * 2)) & 0x03;

        // Escribir en buffer
        gfx[ly * 160 + x] = palette[color];
        
        // Guardar color original para prioridad de sprites
        bg_priority[x] = color_num;
        
        // DEBUG: Contar píxeles no-cero
        total_pixels_checked++;
        if (color_num != 0) non_zero_pixels++;
    }
    
    // Log estadísticas después de un frame completo
    static int frames_logged = 0;
    if (ly == 143 && frames_logged < 5) {
        frames_logged++;
        std::cout << "[PPU BG STATS] Frame " << frames_logged 
                  << ": " << non_zero_pixels << "/" << total_pixels_checked 
                  << " non-zero color pixels\n";
        
        // Sample de píxeles del buffer para verificar
        std::cout << "  Sample pixels (line 72, x=0-15): ";
        for (int i = 0; i < 16; i++) {
            uint32_t pix = gfx[72 * 160 + i];
            // Mostrar qué índice de paleta es
            int pal_idx = -1;
            for (int p = 0; p < 4; p++) {
                if (pix == palette[p]) { pal_idx = p; break; }
            }
            std::cout << pal_idx;
        }
        std::cout << "\n";
        
        non_zero_pixels = 0;
        total_pixels_checked = 0;
    }
}

// ============================================================
// DRAW SPRITES - RENDERIZA LOS SPRITES (OBJ)
// ============================================================
void ppu::draw_sprites() {
    uint8_t lcdc = memory.IO[0x40];
    uint8_t ly = current_line;
    
    // Tamaño del sprite: 8x8 (bit 2 = 0) o 8x16 (bit 2 = 1)
    int sprite_height = (lcdc & 0x04) ? 16 : 8;
    
    // Paletas de sprites
    uint8_t obp0 = memory.IO[0x48];
    uint8_t obp1 = memory.IO[0x49];
    
    // DEBUG: Dump OAM una vez después de que el juego haya cargado
    static int oam_dump_frame = 0;
    static bool oam_dumped = false;
    oam_dump_frame++;
    if (!oam_dumped && oam_dump_frame > 180 && ly == 0) {  // ~3 segundos después del inicio
        oam_dumped = true;
        std::cout << "\n[OAM DUMP] After " << oam_dump_frame << " frames:\n";
        int non_zero_sprites = 0;
        for (int i = 0; i < 40; i++) {
            int addr = i * 4;
            int y = memory.OAM[addr];
            int x = memory.OAM[addr + 1];
            int tile = memory.OAM[addr + 2];
            int flags = memory.OAM[addr + 3];
            if (y != 0 || x != 0) {
                non_zero_sprites++;
                if (non_zero_sprites <= 10) {
                    std::cout << "  Sprite " << i << ": Y=" << y << " X=" << x 
                              << " Tile=0x" << std::hex << tile 
                              << " Flags=0x" << flags << std::dec;
                    // Screen position
                    std::cout << " (Screen: " << (x-8) << "," << (y-16) << ")\n";
                }
            }
        }
        std::cout << "  Total non-zero sprites: " << non_zero_sprites << "/40\n";
        std::cout << "  Sprite height: " << sprite_height << "\n";
        std::cout << "  OBP0=0x" << std::hex << (int)obp0 << " OBP1=0x" << (int)obp1 << std::dec << "\n";
        
        // Dump primeros tiles de VRAM (tiles de sprites en 0x8000)
        std::cout << "\n[VRAM TILE DUMP] First 4 sprite tiles (0x8000-0x803F):\n";
        for (int t = 0; t < 4; t++) {
            std::cout << "  Tile " << t << " (0x" << std::hex << (0x8000 + t*16) << "): ";
            bool all_zero = true;
            for (int b = 0; b < 16; b++) {
                uint8_t byte = memory.VRAM[t * 16 + b];
                std::cout << std::setw(2) << std::setfill('0') << (int)byte << " ";
                if (byte != 0) all_zero = false;
            }
            std::cout << (all_zero ? "(EMPTY)" : "(HAS DATA)") << std::dec << "\n";
        }
        
        // Dump tile 0x2F que es el fondo
        std::cout << "\n[VRAM TILE 0x2F] (Background tile):\n  ";
        for (int b = 0; b < 16; b++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)memory.VRAM[0x2F * 16 + b] << " ";
        }
        std::cout << std::dec << "\n\n";
    }
    
    // Contar sprites visibles en esta línea (max 10)
    int sprites_on_line = 0;
    
    // Estructura temporal para sprites en esta línea
    struct SpriteEntry {
        int x;
        int y;
        int tile;
        int flags;
        int oam_index;
    };
    SpriteEntry line_sprites[10];
    
    // OAM tiene 40 sprites (4 bytes cada uno)
    for (int i = 0; i < 40 && sprites_on_line < 10; i++) {
        // Leer atributos del sprite desde OAM
        int oam_addr = i * 4;
        int y_pos = memory.OAM[oam_addr] - 16;     // Screen Y (OAM Y - 16)
        int x_pos = memory.OAM[oam_addr + 1] - 8;  // Screen X (OAM X - 8)
        int tile_num = memory.OAM[oam_addr + 2];   // Tile number
        int flags = memory.OAM[oam_addr + 3];      // Flags
        
        // Verificar si este sprite está en la línea actual
        if (ly >= y_pos && ly < y_pos + sprite_height) {
            line_sprites[sprites_on_line].x = x_pos;
            line_sprites[sprites_on_line].y = y_pos;
            line_sprites[sprites_on_line].tile = tile_num;
            line_sprites[sprites_on_line].flags = flags;
            line_sprites[sprites_on_line].oam_index = i;
            sprites_on_line++;
        }
    }
    
    // DEBUG: Log detallado de sprites encontrados
    static int sprite_log_count = 0;
    if (sprites_on_line > 0 && sprite_log_count < 20) {
        sprite_log_count++;
        std::cout << "[PPU SPRITES] LY=" << std::dec << (int)ly 
                  << " found " << sprites_on_line << " sprites: ";
        for (int s = 0; s < sprites_on_line && s < 3; s++) {
            std::cout << "[#" << line_sprites[s].oam_index 
                      << " x=" << line_sprites[s].x 
                      << " tile=0x" << std::hex << line_sprites[s].tile << std::dec << "] ";
        }
        std::cout << "\n";
    }
    
    // Ordenar por prioridad X (menor X = mayor prioridad en DMG)
    // Implementación simple: bubble sort (solo 10 elementos max)
    for (int i = 0; i < sprites_on_line - 1; i++) {
        for (int j = i + 1; j < sprites_on_line; j++) {
            if (line_sprites[j].x < line_sprites[i].x) {
                SpriteEntry temp = line_sprites[i];
                line_sprites[i] = line_sprites[j];
                line_sprites[j] = temp;
            }
        }
    }
    
    // Renderizar sprites en orden inverso (menor prioridad primero)
    // para que los de mayor prioridad se dibujen encima
    for (int s = sprites_on_line - 1; s >= 0; s--) {
        int x_pos = line_sprites[s].x;
        int y_pos = line_sprites[s].y;
        int tile_num = line_sprites[s].tile;
        int flags = line_sprites[s].flags;
        
        // Flags del sprite
        bool priority = (flags & 0x80) != 0;  // 0=sobre BG, 1=detrás de BG color 1-3
        bool y_flip = (flags & 0x40) != 0;
        bool x_flip = (flags & 0x20) != 0;
        bool use_obp1 = (flags & 0x10) != 0;
        
        // En modo 8x16, el bit 0 del tile se ignora
        if (sprite_height == 16) {
            tile_num &= 0xFE;
        }
        
        // Calcular qué línea del tile dibujar
        int tile_y = ly - y_pos;
        if (y_flip) {
            tile_y = (sprite_height - 1) - tile_y;
        }
        
        // Si es 8x16 y estamos en la mitad inferior, usar tile+1
        int actual_tile = tile_num;
        if (sprite_height == 16 && tile_y >= 8) {
            actual_tile = tile_num + 1;
            tile_y -= 8;
        }
        
        // Sprites siempre usan direccionamiento 0x8000
        uint16_t tile_addr = 0x8000 + (actual_tile * 16) + (tile_y * 2);
        uint8_t data1 = memory.VRAM[tile_addr - 0x8000];
        uint8_t data2 = memory.VRAM[tile_addr - 0x8000 + 1];
        
        // Seleccionar paleta
        uint8_t palette_data = use_obp1 ? obp1 : obp0;
        
        // Dibujar los 8 píxeles del sprite
        for (int px = 0; px < 8; px++) {
            int screen_x = x_pos + px;
            
            // Fuera de pantalla
            if (screen_x < 0 || screen_x >= 160) continue;
            
            // Calcular bit del color (con posible flip X)
            int color_bit = x_flip ? px : (7 - px);
            
            // Obtener color 0-3
            int color_num = ((data2 >> color_bit) & 1) << 1;
            color_num |= ((data1 >> color_bit) & 1);
            
            // Color 0 es transparente para sprites
            if (color_num == 0) continue;
            
            // Verificar prioridad con fondo
            // Si priority=1 y BG tiene color 1-3, el sprite está detrás
            if (priority && bg_priority[screen_x] != 0) continue;
            
            // Mapear con paleta de sprite (OBP0 o OBP1)
            int color = (palette_data >> (color_num * 2)) & 0x03;
            
            // DEBUG: Contar píxeles de sprite renderizados
            static int sprite_pixels_rendered = 0;
            sprite_pixels_rendered++;
            if (sprite_pixels_rendered <= 10) {
                std::cout << "[SPRITE PIXEL #" << sprite_pixels_rendered << "] "
                          << "x=" << screen_x << " y=" << (int)ly
                          << " color_num=" << color_num << " pal_color=" << color
                          << " tile=0x" << std::hex << actual_tile << std::dec << "\n";
            }
            if (sprite_pixels_rendered == 1000) {
                std::cout << "[SPRITE STATS] 1000 sprite pixels rendered so far!\n";
            }
            
            // Escribir pixel
            gfx[ly * 160 + screen_x] = palette[color];
        }
    }
}

// ============================================================
// CAN ACCESS VRAM - VERIFICAR SI CPU PUEDE ACCEDER A VRAM
// ============================================================
bool ppu::can_access_vram() const {
    // VRAM es accesible excepto durante Modo 3 (Drawing)
    uint8_t lcdc = memory.readMemory(0xFF40);
    if (!(lcdc & 0x80)) return true; // LCD apagado, acceso permitido
    
    return current_mode != 3;
}

// ============================================================
// CAN ACCESS OAM - VERIFICAR SI CPU PUEDE ACCEDER A OAM
// ============================================================
bool ppu::can_access_oam() const {
    // OAM es accesible solo durante Modo 0 (HBlank) y Modo 1 (VBlank)
    uint8_t lcdc = memory.readMemory(0xFF40);
    if (!(lcdc & 0x80)) return true; // LCD apagado, acceso permitido
    
    return (current_mode == 0 || current_mode == 1);
}