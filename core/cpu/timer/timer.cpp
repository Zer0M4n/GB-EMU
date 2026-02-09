#include "timer.h"

timer::timer(mmu& mmu_ref) 
    : memory(mmu_ref), debug_enabled(false), 
      last_tima_logged(0xFF), last_tac_logged(0xFF), last_div_logged(0xFF)
{
    div_counter = 0;
    tima_counter = 0;
}

void timer::reset() {
    div_counter = 0;
    tima_counter = 0;
    
    if (debug_enabled) {
        std::cout << "[TIMER DEBUG] Timer reseteado\n";
    }
}

// ============================================================
// ENABLE DEBUG - ACTIVAR/DESACTIVAR DEBUGGER
// ============================================================
void timer::enable_debug(bool enable) {
    debug_enabled = enable;
    if (debug_enabled) {
        std::cout << "[TIMER DEBUG] Debugger activado\n";
    } else {
        std::cout << "[TIMER DEBUG] Debugger desactivado\n";
    }
}

// ============================================================
// DEBUG TIMER STATE - REPORTE DEL ESTADO DEL TIMER
// ============================================================
void timer::debug_timer_state(const char* event) {
    if (!debug_enabled) return;
    
    uint8_t div = memory.readMemory(0xFF04);
    uint8_t tima = memory.readMemory(0xFF05);
    uint8_t tma = memory.readMemory(0xFF06);
    uint8_t tac = memory.readMemory(0xFF07);
    uint8_t if_reg = memory.readMemory(0xFF0F);
    
    // Parsear información de TAC
    bool timer_enable = (tac & 0x04) != 0;
    uint8_t freq_select = (tac & 0x03);
    
    const char* frequency_names[] = {
        "4096 Hz (1024 cycles)",
        "262144 Hz (16 cycles)",
        "65536 Hz (64 cycles)",
        "16384 Hz (256 cycles)"
    };
    
    // Solo mostrar si hay cambio o evento
    if (event || tima != last_tima_logged || tac != last_tac_logged || div != last_div_logged) {
        std::cout << "\n[TIMER DEBUG] ";
        
        if (event) {
            std::cout << event << " | ";
        }
        
        // Estado del timer
        /*std::cout << "DIV=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)div
                  << " | TIMA=0x" << std::setw(2) << (int)tima
                  << " | TMA=0x" << std::setw(2) << (int)tma
                  << " | TAC=0x" << std::setw(2) << (int)tac
                  << std::dec
                  << " | IF=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)if_reg;
        
        std::cout << std::dec << "\n";
        
        // Información detallada de TAC
        std::cout << "         Timer Enable: " << (timer_enable ? "ON" : "OFF")
                  << " | Frequency: " << frequency_names[freq_select]
                  << " | Internal Counter: DIV=" << div_counter
                  << " | TIMA_cnt=" << tima_counter << "\n";*/
        
        last_tima_logged = tima;
        last_tac_logged = tac;
        last_div_logged = div;
    }
}

// ============================================================
// DEBUG INTERRUPT STATE - REPORTE DEL ESTADO DE LAS INTERRUPCIONES
// ============================================================
void timer::debug_interrupt_state(const char* event) {
    if (!debug_enabled) return;

    uint8_t if_reg = memory.readMemory(0xFF0F);
    uint8_t ie_reg = memory.readMemory(0xFFFF);
    uint8_t tac = memory.readMemory(0xFF07);

    // std::cout << "[TIMER DEBUG] IRQ STATE";
    // if (event) std::cout << " | " << event;
    // std::cout << " | IF=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)if_reg
    //           << " | IE=0x" << std::setw(2) << (int)ie_reg
    //           << " | TAC=0x" << std::setw(2) << (int)tac
    //           << std::dec << "\n";
}

// ============================================================
// STEP - AVANZA EL TIMER (T-CYCLES CORRECTOS)
// ============================================================

void timer::step(int t_cycles) {
    // ============================================================
    // PARTE 1: DIV - SIEMPRE CORRE (FREE-RUNNING)
    // ============================================================
    div_counter += t_cycles;
    
    while (div_counter >= 256) {
        div_counter -= 256;
        memory.IO[0x04]++;  // DIV siempre incrementa
    }
    
    // ============================================================
    // PARTE 2: TIMA - SOLO SI TAC BIT 2 ESTÁ ACTIVO
    // ============================================================
    uint8_t tac = memory.IO[0x07];
    
    if (!(tac & 0x04)) {
        return;  // Timer deshabilitado
    }
    
    // Frecuencia de TIMA según TAC bits 0-1
    int threshold;
    switch (tac & 0x03) {
        case 0: threshold = 1024; break;
        case 1: threshold = 16;   break;
        case 2: threshold = 64;   break;
        case 3: threshold = 256;  break;
        default: threshold = 1024; break;
    }
    
    tima_counter += t_cycles;
    
    while (tima_counter >= threshold) {
        tima_counter -= threshold;
        
        uint8_t tima = memory.IO[0x05];
        
        if (tima == 0xFF) {
            memory.IO[0x05] = memory.IO[0x06];  // TIMA = TMA
            memory.IO[0x0F] |= 0x04;            // Timer IRQ
        } else {
            memory.IO[0x05] = tima + 1;
        }
    }
}