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
    current_mode = 0;
    scanline_dots = 0;
    frame_complete = false;
    prev_stat_line = false;
    
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
    memory.writeMemory(0xFF41, 0x80); // STAT (bit 7 siempre en 1)
}

// ============================================================
// ENABLE DEBUG - ACTIVAR/DESACTIVAR DEBUGGER
// ============================================================
void ppu::enable_debug(bool enable) {
    debug_enabled = enable;
    if (debug_enabled) {
        std::cout << "[PPU DEBUG] Debugger activado\n";
    } else {
        std::cout << "[PPU DEBUG] Debugger desactivado\n";
    }
}

// ============================================================
// DEBUG SCANLINE REPORT - REPORTE DETALLADO DE LA SCANLINE
// ============================================================
void ppu::debug_scanline_report(const char* event) {
    if (!debug_enabled) return;
    
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
    }
}

// ============================================================
// DEBUG MODE CHANGE - REPORTE CUANDO CAMBIA DE MODO
// ============================================================
void ppu::debug_mode_change(uint8_t old_mode, uint8_t new_mode) {
    if (!debug_enabled || old_mode == new_mode) return;
    
    const char* mode_names[] = {"HBlank", "VBlank", "OAM Scan", "Drawing"};
    uint8_t ly = memory.readMemory(0xFF44);
    
    std::cout << "[PPU DEBUG] Cambio de modo en LY=" << std::dec << (int)ly 
              << ": " << mode_names[old_mode] << " → " << mode_names[new_mode]
              << " (dot " << scanline_dots << ")\n";
}

// ============================================================
// STEP - NUEVO SISTEMA DE TIMING PRECISO
// ============================================================
void ppu::step(int cpu_cycles) {
    // Convertir M-cycles de CPU a dots de PPU
    // 1 M-cycle (CPU) = 4 dots (PPU)
    int dots_to_advance = cpu_cycles;
    
    // Avanzar dot por dot para máxima precisión
    for (int i = 0; i < dots_to_advance; i++) {
        step_one_dot();
    }
}

// ============================================================
// STEP ONE DOT - AVANZA UN DOT DE PPU (CORREGIDO)
// ============================================================
void ppu::step_one_dot() {
    // Verificar si el LCD está encendido (Bit 7 de LCDC - 0xFF40)
    uint8_t lcdc = memory.readMemory(0xFF40);
    if (!(lcdc & 0x80)) {
        // LCD apagado: resetear estado
        scanline_dots = 0;
        current_line = 0;
        current_mode = 0;
        memory.writeMemory(0xFF44, 0);
        
        // Poner STAT en modo 0 con bit 7 en 1
        memory.writeMemory(0xFF41, 0x80);
        return;
    }
    
    // LCD encendido: avanzar timing
    dots_counter++;
    scanline_dots++;
    
    // DEBUG: Mostrar cerca del threshold problemático
    if (debug_enabled && scanline_dots >= 250 && scanline_dots <= 255) {
        std::cout << "[PPU DEBUG] THRESHOLD CHECK: LY=" << std::dec << (int)current_line 
                  << " dot=" << scanline_dots << " current_mode=" << (int)current_mode << "\n";
    }
    
    // Determinar modo basado en la línea actual y dots
    uint8_t prev_mode = current_mode;
    
    if (current_line < 144) {
        // Líneas visibles (0-143)
        // Timing correcto de Game Boy DMG:
        // - Dots 0-79: OAM Scan (Modo 2) = 80 dots
        // - Dots 80-251: Drawing (Modo 3) = 172 dots
        // - Dots 252-455: HBlank (Modo 0) = 204 dots
        // Total = 456 dots
        
        if (scanline_dots <= 80) {
            // Modo 2: OAM Scan (dots 1-80)
            current_mode = 2;
            
            // Al entrar en modo 2, verificar interrupción
            if (prev_mode != 2 && scanline_dots == 1) {
                debug_mode_change(prev_mode, 2);
                debug_scanline_report("→ OAM Scan");
                update_stat_interrupt();
            }
        }
        else if (scanline_dots <= 251) {
            // CORREGIDO: Modo 3: Drawing (dots 81-251)
            // Ahora es < 252 en lugar de <= 252
            current_mode = 3;
            
            // Modo 3 no tiene interrupción STAT
            if (prev_mode != 3 && scanline_dots == 81) {
                debug_mode_change(prev_mode, 3);
                debug_scanline_report("→ Drawing");
            }
        }
        else {
            // Modo 0: HBlank (dots 252-455)
            if (current_mode != 0) {
                debug_mode_change(prev_mode, 0);
                current_mode = 0;
                
                // Al entrar en HBlank, renderizar la línea
                draw_scanline();
                debug_scanline_report("→ HBlank (línea renderizada)");
                
                // Verificar interrupción
                update_stat_interrupt();
            }
        }
    }
    else if (current_line < 154) {
        // Líneas 144-153: VBlank
        if (current_mode != 1) {
            debug_mode_change(prev_mode, 1);
            current_mode = 1;
            
            // Al entrar en VBlank (primera vez en línea 144)
            if (current_line == 144 && scanline_dots == 1) {
                uint8_t if_old = memory.readMemory(0xFF0F);
                uint8_t if_new = if_old | 0x01;
                memory.writeMemory(0xFF0F, if_new);

                uint8_t ie_reg = memory.readMemory(0xFFFF);

                frame_complete = true;
                debug_scanline_report("→ VBlank iniciado (Frame completo)");

                if (debug_enabled) {
                    std::cout << "[PPU DEBUG] VBlank IRQ solicitada | IF: 0x"
                              << std::hex << std::setfill('0') << std::setw(2) << (int)if_old
                              << " → 0x" << std::setw(2) << (int)if_new
                              << " | IE=0x" << std::setw(2) << (int)ie_reg
                              << std::dec << "\n";
                }
            }
            
            // Verificar interrupción STAT para VBlank
            update_stat_interrupt();
        }
    }
    
    // Actualizar registro STAT con el modo actual
    update_stat_register();
    
    // Fin de scanline (456 dots)
    if (scanline_dots >= 456) {
        uint8_t old_line = current_line;
        scanline_dots = 0;
        current_line++;
        
        // Actualizar LY
        memory.writeMemory(0xFF44, current_line);
        
        // Debug: reportar fin de línea
        if (debug_enabled) {
            std::cout << "[PPU DEBUG] Fin de scanline: LY=" << std::dec << (int)old_line 
                      << " → " << (int)current_line;
            
            // Alerta si llegamos a VBlank
            if (current_line == 144) {
                std::cout << " *** ENTRANDO A VBLANK ***";
            }
            std::cout << "\n";
        }
        
        // Verificar coincidencia LYC
        check_lyc_coincidence();
        
        // Fin de frame (154 líneas = 0-153)
        if (current_line >= 154) {
            current_line = 0;
            memory.writeMemory(0xFF44, 0);
            frame_complete = false;
            
            if (debug_enabled) {
                std::cout << "\n========== FRAME COMPLETO - RESETEANDO A LY=0 ==========\n\n";
            }
        }
    }
}

// ============================================================
// UPDATE STAT REGISTER - ACTUALIZA BITS DEL REGISTRO STAT
// ============================================================
void ppu::update_stat_register() {
    uint8_t stat = memory.readMemory(0xFF41);
    
    // Limpiar bits 0-1 (modo)
    stat &= 0xFC;
    
    // Establecer nuevo modo
    stat |= (current_mode & 0x03);
    
    // Bit 7 siempre en 1 (no usado)
    stat |= 0x80;
    
    // Escribir de vuelta
    memory.writeMemory(0xFF41, stat);
}

// ============================================================
// CHECK LYC COINCIDENCE - VERIFICAR COINCIDENCIA LY == LYC
// ============================================================
void ppu::check_lyc_coincidence() {
    uint8_t ly = memory.readMemory(0xFF44);
    uint8_t lyc = memory.readMemory(0xFF45);
    uint8_t stat = memory.readMemory(0xFF41);
    
    if (ly == lyc) {
        // Activar bit 2 (Coincidence Flag)
        stat |= 0x04;
        memory.writeMemory(0xFF41, stat);
        
        // Debug
        if (debug_enabled) {
            std::cout << "[PPU DEBUG] LY=LYC Coincidencia detectada en LY=" 
                      << std::dec << (int)ly << "\n";
        }
        
        // Verificar interrupción
        update_stat_interrupt();
    } else {
        // Desactivar bit 2
        stat &= ~0x04;
        memory.writeMemory(0xFF41, stat);
    }
}

// ============================================================
// UPDATE STAT INTERRUPT - EDGE-TRIGGERED STAT INTERRUPT
// ============================================================
void ppu::update_stat_interrupt() {
    uint8_t stat = memory.readMemory(0xFF41);
    uint8_t ly = memory.readMemory(0xFF44);
    uint8_t lyc = memory.readMemory(0xFF45);
    
    bool current_stat_line = false;
    const char* interrupt_reason = "";
    
    // Verificar todas las fuentes de interrupción STAT
    
    // Bit 6: LYC=LY Interrupt Enable
    if ((stat & 0x40) && (ly == lyc)) {
        current_stat_line = true;
        interrupt_reason = "LY=LYC";
    }
    
    // Bit 5: Mode 2 (OAM) Interrupt Enable
    if ((stat & 0x20) && (current_mode == 2)) {
        current_stat_line = true;
        interrupt_reason = "Mode 2 (OAM)";
    }
    
    // Bit 4: Mode 1 (VBlank) Interrupt Enable
    if ((stat & 0x10) && (current_mode == 1)) {
        current_stat_line = true;
        interrupt_reason = "Mode 1 (VBlank)";
    }
    
    // Bit 3: Mode 0 (HBlank) Interrupt Enable
    if ((stat & 0x08) && (current_mode == 0)) {
        current_stat_line = true;
        interrupt_reason = "Mode 0 (HBlank)";
    }
    
    // EDGE-TRIGGERED: Solo disparar en transición LOW → HIGH
    if (current_stat_line && !prev_stat_line) {
        uint8_t if_old = memory.readMemory(0xFF0F);
        uint8_t if_new = if_old | 0x02;
        memory.writeMemory(0xFF0F, if_new);

        if (debug_enabled) {
            uint8_t ie_reg = memory.readMemory(0xFFFF);
            std::cout << "[PPU DEBUG] Interrupción STAT solicitada: " << interrupt_reason 
                      << " (LY=" << std::dec << (int)ly << ")\n";
            std::cout << "[PPU DEBUG] STAT IRQ | IF: 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << (int)if_old << " → 0x" << std::setw(2) << (int)if_new
                      << " | IE=0x" << std::setw(2) << (int)ie_reg
                      << std::dec << "\n";
        }
    }
    
    prev_stat_line = current_stat_line;
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