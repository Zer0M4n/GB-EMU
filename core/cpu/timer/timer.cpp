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
static int timer_log_counter = 0;

void timer::step(int t_cycles) {
    // ============================================================
    // DIV (0xFF04) - Incrementa cada 256 T-cycles
    // ============================================================
    // DIV incrementa cada 256 T-cycles (16384 Hz)
    // Cualquier escritura a DIV lo resetea a 0
    
    uint8_t& div_reg = memory.IO[0x04];
    
    div_counter += t_cycles;

    while (div_counter >= 256) {
        div_counter -= 256;
        div_reg++;
        // DIV overflow silencioso (no genera interrupción)
    }

    // ============================================================
    // TIMA (0xFF05) - Timer configurable
    // ============================================================
    uint8_t tac = memory.IO[0x07];
    
    // Log periódico del estado del TAC (cada ~1 segundo de emulación)
    timer_log_counter += t_cycles;
    if (debug_enabled && timer_log_counter >= 4194304) {  // ~1 segundo
        timer_log_counter = 0;
        std::cout << "[TIMER] TAC=0x" << std::hex << (int)tac 
                  << " TIMA=0x" << (int)memory.IO[0x05]
                  << " TMA=0x" << (int)memory.IO[0x06]
                  << " IF=0x" << (int)memory.IO[0x0F]
                  << " Timer " << ((tac & 0x04) ? "ENABLED" : "DISABLED")
                  << std::dec << "\n";
    }
    
    // Bit 2 de TAC = Timer Enable
    if (!(tac & 0x04)) {
        // Timer deshabilitado - no hacer nada
        return;
    }
    
    // Determinar umbral según bits 0-1 de TAC
    // IMPORTANTE: Estos valores están en T-cycles
    int threshold;
    switch (tac & 0x03) {
        case 0: threshold = 1024; break;  // 4096 Hz   (CPU_FREQ / 1024)
        case 1: threshold = 16;   break;  // 262144 Hz (CPU_FREQ / 16)
        case 2: threshold = 64;   break;  // 65536 Hz  (CPU_FREQ / 64)
        case 3: threshold = 256;  break;  // 16384 Hz  (CPU_FREQ / 256)
        default: threshold = 1024; break;
    }

    tima_counter += t_cycles;

    while (tima_counter >= threshold) {
        tima_counter -= threshold;

        uint8_t& tima = memory.IO[0x05];
        
        if (tima == 0xFF) {
            // TIMA overflow: resetear a TMA y solicitar interrupción
            tima = memory.IO[0x06]; // TMA
            
            // CRÍTICO: Usar OR para no sobrescribir otras interrupciones
            // Bit 2 de IF = Timer Interrupt
            memory.IO[0x0F] |= 0x04;
            
            if (debug_enabled) {
                std::cout << "[TIMER] Overflow! TIMA reset to TMA=0x" 
                          << std::hex << (int)memory.IO[0x06]
                          << ", Timer IRQ requested (IF |= 0x04)\n";
            }
        } else {
            tima++;
        }
    }
}