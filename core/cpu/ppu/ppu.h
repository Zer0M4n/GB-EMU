#pragma once
#include <cstdint>
#include <vector>
#include "mmu.h"   // Ajustá el path si es necesario

class ppu
{
public:
    explicit ppu(mmu& mmu_ref);

    void step(int cpu_cycles);
    void enable_debug(bool enable);

    // Estado visible para el emulador principal
    bool     frame_complete;
    std::vector<uint32_t> gfx;   // 160 * 144 píxeles (ABGR)

    // Acceso a memoria (para que el MMU consulte bloqueos)
    bool can_access_vram() const;
    bool can_access_oam()  const;

    int  current_mode;
    int  current_line;

private:
    mmu& memory;

    // Timing
    int  dots_counter;
    int  scanline_dots;

    // Estado de interrupción STAT (Wired-OR)
    bool prev_stat_line;
    bool vblank_irq_fired;

    // Window: contador interno de líneas dibujadas (independiente de LY)
    int  window_line_counter;

    // Paleta DMG (4 colores)
    uint32_t palette[4];

    // Prioridad de BG/Window por columna (para sprites)
    uint8_t bg_priority[160];

    // Debug
    bool    debug_enabled;
    uint8_t last_line_logged;
    uint8_t last_mode_logged;

    // Métodos internos
    void step_one_dot();
    void check_lyc_coincidence();
    void update_stat_interrupt();
    void draw_scanline();
    void draw_background();
    void draw_window();       // ← NUEVO
    void draw_sprites();
    void debug_scanline_report(const char* event = nullptr);
    void debug_mode_change(uint8_t old_mode, uint8_t new_mode);
};