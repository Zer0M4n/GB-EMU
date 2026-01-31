#include <iostream>
#include <string>
#include <vector>
#include <emscripten.h>

#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"
#include "core/cpu/ppu/ppu.h" 

// Punteros globales
cpu* global_cpu = nullptr;
ppu* global_ppu = nullptr;
mmu* global_mmu = nullptr;

// --- EXPORTAR BUFFER A JS ---
extern "C" {
    // Devuelve el puntero al inicio del array de píxeles (uint32_t = RGBA)
    // El tamaño es siempre 160 * 144 * 4 bytes
    uint8_t* get_video_buffer() {
        if (global_ppu) {
            // .data() devuelve el puntero al array interno del vector
            return reinterpret_cast<uint8_t*>(global_ppu->gfx.data());
        }
        return nullptr;
    }
    
    // Función auxiliar para saber el tamaño (útil para JS)
    int get_video_buffer_size() {
        return 160 * 144 * 4; 
    }
}

// Bucle principal (se ejecuta 60 veces por segundo gracias a Emscripten)
void main_loop() {
    if (!global_cpu || !global_ppu) return;

    // 1. CORRECCIÓN DE TIMING
    // La Game Boy funciona a 4,194,304 Hz.
    // 4194304 / 60 FPS ≈ 69905 ciclos por frame (redondeamos a 70224 para sincronía vertical exacta)
    const int CYCLES_PER_FRAME = 70224; 
    int cycles_this_frame = 0;

    // Ejecutamos instrucciones hasta completar el tiempo de un frame (16.7ms)
    while (cycles_this_frame < CYCLES_PER_FRAME) {
        
        // A. La CPU ejecuta UNA instrucción
        int cycles = global_cpu->step();
        
        // B. La PPU avanza el mismo tiempo
        global_ppu->step(cycles);

        // C. IMPORTANTE: Revisar Interrupciones
        // La PPU enciende bits en 0xFF0F (VBlank). La CPU debe reaccionar.
        // Si no tienes esta función en tu CPU, asegúrate de implementarla o 
        // de que tu función 'step()' la llame internamente.
        // global_cpu->handle_interrupts(); 

        // D. Sumamos al contador del frame
        cycles_this_frame += cycles;
    }

    // --- AVISAR A JS QUE DIBUJE ---
    // Llamamos a JS solo cuando el frame está COMPLETO
    EM_ASM({
        // Asumimos que esta función existe en tu HTML/JS
        drawCanvas();
    });
}

int main(int argc, char **argv) {
    std::cout << "--- Game Boy Emulator Booting WASM ---" << "\n";

std::string romPath = "/roms/tetris.gb";

    try {
        global_mmu = new mmu(romPath);
        global_ppu = new ppu(*global_mmu); 
        
        // Pasamos MMU al CPU. 
        // IDEALMENTE: El CPU debería tener acceso al PPU también para ciertos hacks,
        // pero por ahora con compartir la MMU es suficiente.
        global_cpu = new cpu(*global_mmu); 
        
        std::cout << "Hardware inicializado. Iniciando Main Loop...\n";

        // Iniciamos el bucle infinito controlado por el navegador
        // 0 fps = usar requestAnimationFrame del navegador (lo más suave)
        // 1 = simular bucle infinito
        emscripten_set_main_loop(main_loop, 0, 1);
        
    } catch (const std::exception& e) {
        std::cout << "Error fatal: " << e.what() << "\n";
        // En caso de error, podríamos intentar cargar una ROM de prueba o salir
    }

    return 0;
}