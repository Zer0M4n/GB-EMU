#include <iostream>
#include <string>
#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Variables globales para el contexto Web
cpu* global_cpu = nullptr;
int debug_steps = 0; // Contador de pasos para detener el bucle

// Esta función se llama 60 veces por segundo (aprox)
void main_loop() {
    if (global_cpu) {
        // Ejecutamos 1 instrucción
        global_cpu->step();
        
        // Aumentamos contador
        debug_steps++;

        // --- FRENO DE EMERGENCIA PARA DEBUG ---
        // Si llegamos a 50 pasos, detenemos el emulador web
        if (debug_steps >= 50) {
            std::cout << "--- Límite de prueba alcanzado (50 pasos). Deteniendo loop. ---\n";
            #ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop(); // <--- ESTO DETIENE EL BUCLE INFINITO
            #endif
        }
    }
}

int main(int argc, char **argv) {
    std::cout << "--- Iniciando Emulador Game Boy (Web/Desktop) ---" << "\n";

    std::string romPath = "roms/tetris.gb"; 

    if (argc > 1) {
        romPath = argv[1];
    } else {
        std::cout << "Info: Cargando ROM por defecto: " << romPath << "\n";
    }

    try {
        static mmu memoryBus(romPath);
        static cpu processor(memoryBus);
        
        global_cpu = &processor;
        
        std::cout << "Sistema listo. CPU arrancando...\n";

        #ifdef __EMSCRIPTEN__
            // Iniciamos el bucle web
            emscripten_set_main_loop(main_loop, 0, 1);
        #else
            // Bucle Desktop
            for(int i = 0; i < 50; i++) {
                processor.step();
            }
        #endif

    } catch (const std::exception& e) {
        std::cerr << "Error Fatal: " << e.what() << "\n";
        return -1;
    }

    return 0;
}