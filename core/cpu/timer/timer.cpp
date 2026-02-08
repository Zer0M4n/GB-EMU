#include "timer.h"

timer::timer(mmu& mmu_ref) : memory(mmu_ref) {
    div_counter = 0;
    tima_counter = 0;
}

void timer::reset() {
    div_counter = 0;
    tima_counter = 0;
}

void timer::step(int cycles) {
    // 1. Registro DIV (0xFF04)
    // Incrementa a una frecuencia de 16384Hz (CPU / 256)
    div_counter += cycles;
    while (div_counter >= 256) {
        div_counter -= 256;
        uint8_t div = memory.readMemory(0xFF04);
        memory.writeMemory(0xFF04, div + 1);
    }

    // 2. Registro TIMA (0xFF05)
    uint8_t tac = memory.readMemory(0xFF07);
    
    // Bit 2: Timer Enable
    if (tac & 0x04) { 
        int threshold = 1024; // Default 00: 4096Hz (1024 cycles)
        
        switch (tac & 0x03) {
            case 0: threshold = 1024; break; // 4096 Hz
            case 1: threshold = 16;   break; // 262144 Hz
            case 2: threshold = 64;   break; // 65536 Hz
            case 3: threshold = 256;  break; // 16384 Hz
        }

        tima_counter += cycles;
        
        while (tima_counter >= threshold) {
            tima_counter -= threshold;
            
            uint8_t tima = memory.readMemory(0xFF05);
            
            // Overflow: TIMA pasa de 0xFF a 0x00
            if (tima == 0xFF) {
                // 1. Recargar TIMA con valor de TMA (0xFF06)
                uint8_t tma = memory.readMemory(0xFF06);
                memory.writeMemory(0xFF05, tma);
                
                // 2. Solicitar Interrupción de Timer (Bit 2)
                uint8_t if_reg = memory.readMemory(0xFF0F);
                memory.writeMemory(0xFF0F, if_reg | 0x04);
            } else {
                memory.writeMemory(0xFF05, tima + 1);
            }
        }
    }
}