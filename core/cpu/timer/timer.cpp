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
// ARQUITECTURA DEL TIMER DE GAME BOY:
// 
// DIV (0xFF04): Contador de libre ejecución. SIEMPRE incrementa
//               cada 256 T-cycles (16384 Hz). NO depende de TAC.
//               Cualquier escritura lo resetea a 0.
//
// TIMA (0xFF05): Timer configurable. SOLO incrementa si TAC bit 2 = 1.
//                La frecuencia depende de TAC bits 0-1.
//                Al hacer overflow (0xFF→0x00), genera interrupción
//                y se recarga con el valor de TMA.
//
// TMA (0xFF06): Valor de recarga para TIMA tras overflow.
//
// TAC (0xFF07): Bit 2 = Timer Enable
//               Bits 0-1 = Frecuencia de TIMA
// ============================================================

static int timer_log_counter = 0;
static uint8_t last_div_logged = 0;

void timer::step(int t_cycles) {
    // ============================================================
    // PARTE 1: DIV - SIEMPRE CORRE (FREE-RUNNING)
    // ============================================================
    // DIV incrementa cada 256 T-cycles independientemente de TAC
    
    div_counter += t_cycles;
    
    while (div_counter >= 256) {
        div_counter -= 256;
        memory.IO[0x04]++;  // DIV siempre incrementa
    }
    
    // ============================================================
    // DEBUG: Log periódico (cada ~1 segundo emulado)
    // ============================================================
    timer_log_counter += t_cycles;
    if (debug_enabled && timer_log_counter >= 4194304) {
        timer_log_counter = 0;
        uint8_t current_div = memory.IO[0x04];
        uint8_t tac = memory.IO[0x07];
        
        std::cout << "[TIMER] DIV=0x" << std::hex << (int)current_div
                  << " (delta=" << std::dec << (int)(uint8_t)(current_div - last_div_logged) << ")"
                  << " TAC=0x" << std::hex << (int)tac 
                  << " TIMA=0x" << (int)memory.IO[0x05]
                  << " TMA=0x" << (int)memory.IO[0x06]
                  << " IF=0x" << (int)memory.IO[0x0F]
                  << " Timer " << ((tac & 0x04) ? "ENABLED" : "DISABLED")
                  << std::dec << "\n";
        
        last_div_logged = current_div;
    }
    
    // ============================================================
    // PARTE 2: TIMA - SOLO SI TAC BIT 2 ESTÁ ACTIVO
    // ============================================================
    uint8_t tac = memory.IO[0x07];
    
    // Si Timer está deshabilitado (TAC bit 2 = 0), no procesar TIMA
    if (!(tac & 0x04)) {
        return;  // DIV ya se actualizó arriba, solo salir
    }
    
    // Determinar frecuencia de TIMA según TAC bits 0-1
    // Valores en T-cycles entre cada incremento de TIMA
    int threshold;
    switch (tac & 0x03) {
        case 0: threshold = 1024; break;  // 4096 Hz   (4194304 / 1024)
        case 1: threshold = 16;   break;  // 262144 Hz (4194304 / 16)
        case 2: threshold = 64;   break;  // 65536 Hz  (4194304 / 64)
        case 3: threshold = 256;  break;  // 16384 Hz  (4194304 / 256)
        default: threshold = 1024; break;
    }
    
    tima_counter += t_cycles;
    
    while (tima_counter >= threshold) {
        tima_counter -= threshold;
        
        uint8_t tima = memory.IO[0x05];
        
        if (tima == 0xFF) {
            // TIMA overflow: recargar con TMA y solicitar interrupción
            memory.IO[0x05] = memory.IO[0x06];  // TIMA = TMA
            memory.IO[0x0F] |= 0x04;            // IF bit 2 = Timer IRQ
            
            if (debug_enabled) {
                std::cout << "[TIMER] Overflow! TIMA reset to TMA=0x" 
                          << std::hex << (int)memory.IO[0x06]
                          << ", Timer IRQ requested (IF |= 0x04)\n";
            }
        } else {
            memory.IO[0x05] = tima + 1;
        }
    }
}