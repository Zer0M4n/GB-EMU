#include <iostream>
#include <string>
#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"

// 1. Incluir cabecera de Emscripten solo si estamos compilando para web
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Puntero global necesario para que la función estática de WASM vea la CPU
cpu* global_cpu = nullptr;

// Función de bucle para el navegador (ejecuta 1 frame y devuelve el control)
void main_loop() {
    if(global_cpu) {
        // Ejecutar instrucciones equivalentes a un frame (aprox 70,000 ciclos)
        // Por ahora ponemos 1 paso para probar
        global_cpu->step(); 
    }
}

int main(int argc, char **argv) {
    std::string romPath = "tetris.gb"; // Nombre fijo para la prueba web
    
    // Si hay argumentos (Desktop), úsalos. En Web, usaremos el archivo precargado.
    if (argc > 1) romPath = argv[1];

    try {
        static mmu memoryBus(romPath); // static para que no muera al salir de main
        static cpu processor(memoryBus);
        global_cpu = &processor; // Guardamos referencia para el loop

        std::cout << "Sistema listo. Iniciando...\n";

        #ifdef __EMSCRIPTEN__
            // 2. MODO WEB: Le decimos al navegador qué función llamar repetidamente
            // 0 = fps por defecto (requestAnimationFrame), 1 = loop infinito simulado
            emscripten_set_main_loop(main_loop, 0, 1);
        #else
            // 3. MODO DESKTOP (Tu código original)
            while(true) {
                processor.step();
            }
        #endif

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}