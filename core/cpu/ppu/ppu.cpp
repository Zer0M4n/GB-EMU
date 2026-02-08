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
    std::fill(gfx.begin(), gfx.end(), 0xFF0FBC9B); // Color base verde claro

    // Paleta clásica "Game Boy" (Formato 0xAARRGGBB)
    palette[0] = 0xFF0FBC9B; // Blanco/Verde claro
    palette[1] = 0xFF0FAC8B; // Verde claro medio
    palette[2] = 0xFF306230; // Verde oscuro medio
    palette[3] = 0xFF0F380F; // Negro/Verde oscuro
    
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
    uint8_t lcdc = memory.readMemory(0xFF40);
    uint8_t ly = memory.readMemory(0xFF44);
    
    if (debug_enabled) {
        std::cout << "[PPU DEBUG] Renderizando scanline " << std::dec << (int)ly << "\n";
    }
    
    // Verificar si el fondo está activado (Bit 0 LCDC)
    if (lcdc & 0x01) {
        draw_background();
    }
    
    // TODO: Añadir renderizado de sprites (Bit 1 LCDC)
    // TODO: Añadir renderizado de ventana (Bit 5 LCDC)
}

// ============================================================
// DRAW BACKGROUND - RENDERIZA EL FONDO
// ============================================================
void ppu::draw_background() {
    uint8_t lcdc = memory.readMemory(0xFF40);
    uint8_t scy = memory.readMemory(0xFF42);
    uint8_t scx = memory.readMemory(0xFF43);
    uint8_t ly = memory.readMemory(0xFF44);
    uint8_t bgp = memory.readMemory(0xFF47); // Paleta Fondo

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
        
        // Índice del tile
        uint8_t tile_num = memory.readMemory(tile_addr);
        
        // CALCULAR DIRECCIÓN DEL TILE DATA
        uint16_t tile_location = 0;
        if (signed_tile_addressing) {
            // Modo 0: 0x9000 base + índice con signo
            tile_location = 0x9000 + ((int8_t)tile_num * 16);
        } else {
            // Modo 1: 0x8000 base + índice sin signo
            tile_location = 0x8000 + (tile_num * 16);
        }

        // Qué línea vertical del tile (0-7) * 2 bytes
        uint8_t line = (y_pos % 8) * 2;
        
        uint8_t data1 = memory.readMemory(tile_location + line);
        uint8_t data2 = memory.readMemory(tile_location + line + 1);

        // Bit del color (invertido, bit 7 es pixel 0)
        int color_bit = 7 - (x_pos % 8);
        
        // Combinar bits
        int color_num = ((data2 >> color_bit) & 1) << 1;
        color_num |= ((data1 >> color_bit) & 1);

        // Mapear con paleta (BGP)
        int color = (bgp >> (color_num * 2)) & 0x03;

        // Escribir en buffer
        gfx[ly * 160 + x] = palette[color];
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