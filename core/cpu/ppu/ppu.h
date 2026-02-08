// ============================================================
// PPU.H - CON DEBUGGER INCRUSTADO PARA MONITOREAR SCANLINE
// ============================================================

#ifndef PPU_H
#define PPU_H
#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include "mmu/mmu.h"

class ppu {
public:
    ppu(mmu& mmu_ref);
    void step(int cpu_cycles);

    // Buffer de píxeles: 160 de ancho x 144 de alto
    // Usamos uint32_t para formato RGBA o ARGB directo (0xFFRRGGBB)
    std::vector<uint32_t> gfx; 
    
    // Bandera para saber si el frame terminó y se puede pintar en pantalla
    bool frame_complete;
    
    // NUEVAS FUNCIONES PÚBLICAS para verificar acceso a memoria
    bool can_access_vram() const;
    bool can_access_oam() const;
    
    // ============================================================
    // FUNCIONES DE DEBUGGING
    // ============================================================
    void enable_debug(bool enable = true);
    void debug_scanline_report(const char* event = nullptr);
    void debug_mode_change(uint8_t old_mode, uint8_t new_mode);

private:
    mmu& memory;
    
    // ============================================================
    // VARIABLES DE TIMING (NUEVAS)
    // ============================================================
    uint64_t dots_counter;      // Contador total de dots desde inicio
    uint16_t scanline_dots;     // Dots en la línea actual (0-455)
    uint8_t current_line;       // Línea actual (0-153)
    uint8_t current_mode;       // Modo actual (0-3)
    bool prev_stat_line;        // Estado anterior de STAT line (para edge detection)
    
    // Paleta de colores
    uint32_t palette[4];
    
    // ============================================================
    // VARIABLES DE DEBUGGING
    // ============================================================
    bool debug_enabled;         // Habilitar/deshabilitar debug
    uint8_t last_line_logged;   // Última línea registrada en debug
    uint8_t last_mode_logged;   // Último modo registrado en debug

    // ============================================================
    // FUNCIONES PRIVADAS (NUEVAS Y ACTUALIZADAS)
    // ============================================================
    void step_one_dot();              // Avanza un dot de PPU
    void update_stat_register();      // Actualiza bits del registro STAT
    void update_stat_interrupt();     // Maneja interrupción STAT (edge-triggered)
    void check_lyc_coincidence();     // Verifica coincidencia LY == LYC
    
    void draw_scanline();             // Renderiza una línea completa
    void draw_background();           // Renderiza el fondo
    // TODO: void draw_sprites();     // Renderizar sprites
    // TODO: void draw_window();      // Renderizar ventana
};

#endif // PPU_H