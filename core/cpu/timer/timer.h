#pragma once
#include <cstdint>
#include <iostream>
#include <iomanip>
#include "mmu/mmu.h"

class timer {
public:
    timer(mmu& mmu_ref);
    void step(int cycles);
    void reset();
    
    // ============================================================
    // FUNCIONES DE DEBUGGING
    // ============================================================
    void enable_debug(bool enable = true);
    void debug_timer_state(const char* event = nullptr);
    void debug_interrupt_state(const char* event = nullptr);

private:
    mmu& memory;
    int div_counter;
    int tima_counter;
    
    // ============================================================
    // VARIABLES DE DEBUGGING
    // ============================================================
    bool debug_enabled;
    uint8_t last_tima_logged;
    uint8_t last_tac_logged;
    uint8_t last_div_logged;
};