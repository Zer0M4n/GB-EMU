#include <iostream>
#include <string>
#include <emscripten.h>

#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"
#include "core/cpu/ppu/ppu.h" // <--- Incluir PPU

// Punteros globales
cpu* global_cpu = nullptr;
ppu* global_ppu = nullptr; // <--- Puntero PPU
mmu* global_mmu = nullptr;

// --- EXPORTAR BUFFER A JS ---
// Esta función devuelve la dirección de memoria donde empiezan los píxeles
extern "C" {
    uint8_t* get_video_buffer() {
        if (global_ppu) {
            // Reinterpretamos el array de uint32_t como puntero a bytes puros
            return reinterpret_cast<uint8_t*>(global_ppu->gfx.data());
        }
        return nullptr;
    }
}

// Bucle principal (se ejecuta ~60 veces por segundo)
void main_loop() {
    if (!global_cpu || !global_ppu) return;

    // La Game Boy corre a 4,194,304 Hz.
    // A 60 FPS, eso son aprox 70224 ciclos por frame.
    const int CYCLES_PER_FRAME = 70224;
    int cycles_this_frame = 0;

    // Ejecutamos instrucciones hasta completar el tiempo de un frame
    while (cycles_this_frame < CYCLES_PER_FRAME) {
        
        // 1. La CPU avanza
        int cycles = global_cpu->step();
        
        // 2. La PPU avanza la misma cantidad de tiempo
        global_ppu->step(cycles);

        // 3. Sumamos al contador del frame
        cycles_this_frame += cycles;
    }

    // --- AVISAR A JS QUE DIBUJE ---
    // Llamamos a una función JavaScript llamada 'drawCanvas' que definiremos en el HTML
    EM_ASM({
        drawCanvas();
    });
}

int main(int argc, char **argv) {
    
    std::cout << "--- Game Boy Emulator Booting WASM ---" << "\n";

    std::cout << "--- Game Boy Emulator Booting ---" << "\n";

    std::string romPath = "roms/tetris.gb"; // Asegúrate que la ROM exista

    try {
        // Inicializamos los componentes en orden
        // Usamos 'new' para que vivan en el Heap y no se borren al salir del main
        global_mmu = new mmu(romPath);
        global_ppu = new ppu(*global_mmu); // PPU necesita referencia a MMU
        global_cpu = new cpu(*global_mmu); // CPU necesita referencia a MMU (y pronto a PPU para interrupciones)
        
        std::cout << "Hardware inicializado correctamente.\n";

        // Iniciamos el bucle infinito del navegador
        emscripten_set_main_loop(main_loop, 0, 1);
        
    } catch (const std::exception& e) {
        std::cout << "Error fatal: " << e.what() << "\n";
    }

    return 0;
}