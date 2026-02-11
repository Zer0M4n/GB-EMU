// ============================================================
// EMC_MAIN.CPP - VERSIÓN DINÁMICA (CARGA DE ARCHIVOS USUARIO)
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include <emscripten.h>

#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"
#include "core/cpu/ppu/ppu.h"
#include "core/cpu/timer/timer.h"
#include "core/cpu/APU/apu.h"

// Punteros globales
cpu* global_cpu = nullptr;
ppu* global_ppu = nullptr;
mmu* global_mmu = nullptr;
timer* global_timer = nullptr;
APU* global_apu = nullptr;

// Estado del sistema
bool is_game_loaded = false;
bool audio_muted = false;

// --- FUNCIÓN DE LIMPIEZA ---
// Borra la memoria del juego anterior antes de cargar uno nuevo
void reset_emulator() {
    is_game_loaded = false;

    if (global_cpu) { delete global_cpu; global_cpu = nullptr; }
    if (global_timer) { delete global_timer; global_timer = nullptr; }
    if (global_ppu) { delete global_ppu; global_ppu = nullptr; }
    if (global_apu) { delete global_apu; global_apu = nullptr; }
    
    // MMU se borra al final porque otros componentes dependen de él en su destructor
    if (global_mmu) { delete global_mmu; global_mmu = nullptr; }
    
    std::cout << "[C++] Memoria liberada. Listo para cargar ROM.\n";
}

extern "C" {
    // --- VIDEO ---
    uint8_t* get_video_buffer() {
        if (global_ppu) return reinterpret_cast<uint8_t*>(global_ppu->gfx.data());
        return nullptr;
    }
    
    int get_video_buffer_size() { return 160 * 144 * 4; }
    
    // --- INPUT ---
    void set_button(int button_id, bool pressed) {
        if (global_mmu) global_mmu->setButton(button_id, pressed);
    }
    
    // --- AUDIO ---
    float* get_audio_buffer() {
        if (global_apu) return global_apu->getBufferPointer();
        return nullptr;
    }
    
    int get_audio_samples_available() {
        if (global_apu) return global_apu->getSamplesAvailable();
        return 0;
    }
    
    int fill_audio_buffer(int maxSamples) {
        if (global_apu) return global_apu->fillOutputBuffer(maxSamples);
        return 0;
    }
    
    void set_audio_muted(bool muted) {
        audio_muted = muted;
    }

    // ============================================================
    // NUEVA FUNCIÓN: CARGAR ROM DESDE JS
    // ============================================================
    // Recibe la ruta del archivo (string) que JS escribió en el FS virtual
    int load_rom_from_js(char* filename) {
        std::cout << "[C++] Solicitud recibida para cargar: " << filename << "\n";
        
        reset_emulator(); // Limpiar lo viejo

        try {
            std::string romPath(filename);

            // 1. Inicializar MMU (Carga el archivo desde el FS virtual)
            global_mmu = new mmu(romPath);
            
            // 2. Inicializar resto de componentes
            const bool enable_debug = false; // Debug off para mejor rendimiento en web
            
            global_ppu = new ppu(*global_mmu);
            global_ppu->enable_debug(enable_debug);
            
            global_timer = new timer(*global_mmu);
            global_timer->enable_debug(enable_debug);
            
            global_cpu = new cpu(*global_mmu);
            
            global_apu = new APU();
            global_mmu->setAPU(global_apu);
            
            std::cout << "[C++] Componentes inicializados. Juego arrancando...\n";
            is_game_loaded = true;
            return 1; // Éxito

        } catch (const std::exception& e) {
            std::cerr << "[C++] ERROR FATAL cargando ROM: " << e.what() << "\n";
            is_game_loaded = false;
            return 0; // Fallo
        }
    }
}

// ============================================================
// MAIN LOOP
// ============================================================
void main_loop() {
    // Si no hay juego cargado, no hacemos nada (CPU idle)
    if (!is_game_loaded || !global_cpu) {
        return; 
    }

    const int T_CYCLES_PER_FRAME = 70224;
    int t_cycles_this_frame = 0;

    while (t_cycles_this_frame < T_CYCLES_PER_FRAME) {
        int cpu_t_cycles = global_cpu->step();
        if (cpu_t_cycles < 4) cpu_t_cycles = 4;
        
        global_ppu->step(cpu_t_cycles);
        global_timer->step(cpu_t_cycles);
        
        if (!audio_muted) {
            global_apu->tick(cpu_t_cycles);
        }
        
        t_cycles_this_frame += cpu_t_cycles;
    }

    // Dibujar pantalla
    EM_ASM({
        if (typeof drawCanvas === 'function') {
            drawCanvas();
        }
    });
}

// ============================================================
// MAIN - PUNTO DE ENTRADA
// ============================================================
int main(int argc, char **argv) {
    std::cout << "--- Game Boy Emulator (WASM) ---\n";
    std::cout << "--- Esperando archivo ROM desde JavaScript ---\n";

    // Ya NO cargamos el juego aquí.
    // Solo configuramos el bucle principal.
    
    // 0 = requestAnimationFrame, 1 = simular bucle infinito
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}