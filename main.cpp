#include <iostream>
#include <string>
#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"

int main(int argc, char **argv) {
    std::cout << "--- Iniciando Emulador Game Boy ---" << "\n";

    // Verificamos si el usuario pasó un archivo
    if (argc < 2) {
        std::cerr << "Uso: ./gb-emu <ruta_al_juego.gb>\n";
        return -1;
    }

    std::string romPath = argv[1]; // Tomamos el argumento de la consola

    try {
        // 1. MMU carga la ROM
        mmu memoryBus(romPath);

        // 2. CPU se conecta a la MMU
        cpu processor(memoryBus);
        
        std::cout << "Sistema listo. CPU arrancando...\n";

        // Bucle infinito (por ahora lo limitamos para no spamear tu terminal)
        // En el futuro esto será while(true)
        for(int i = 0; i < 100; i++) {
            processor.step();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error Fatal: " << e.what() << "\n";
        return -1;
    }

    return 0;
}