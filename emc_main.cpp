// ============================================================
// EMC_MAIN.CPP - VERSIÓN MEJORADA CON TIMER
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include <emscripten.h>

#include "core/cpu/mmu/mmu.h"
#include "core/cpu/cpu.h"
#include "core/cpu/ppu/ppu.h"
#include "core/cpu/timer/timer.h" // AÑADIDO: Timer

// Punteros globales
cpu* global_cpu = nullptr;
ppu* global_ppu = nullptr;
mmu* global_mmu = nullptr;
timer* global_timer = nullptr; // NUEVO

// --- EXPORTAR BUFFER A JS ---
extern "C" {
    // Devuelve el puntero al inicio del array de píxeles (uint32_t = RGBA)
    uint8_t* get_video_buffer() {
        if (global_ppu) {
            return reinterpret_cast<uint8_t*>(global_ppu->gfx.data());
        }
        return nullptr;
    }
    
    int get_video_buffer_size() {
        return 160 * 144 * 4; 
    }
    
    // NUEVA: Función para establecer estado de botones
    void set_button(int button_id, bool pressed) {
        if (global_mmu) {
            global_mmu->setButton(button_id, pressed);
        }
    }
}

// ============================================================
// MAIN LOOP - SINCRONIZACIÓN PRECISA CON T-CYCLES
// ============================================================
void main_loop() {
    if (!global_cpu || !global_ppu || !global_timer) return;

    // TIMING GAME BOY:
    // - CPU: 4,194,304 Hz (T-cycles por segundo)
    // - 1 Frame = 70224 T-cycles (a ~59.73 FPS)
    // - 1 T-cycle = 1 dot de PPU
    // - 1 M-cycle = 4 T-cycles
    
    const int T_CYCLES_PER_FRAME = 70224;
    int t_cycles_this_frame = 0;

    // Ejecutar hasta completar un frame completo
    while (t_cycles_this_frame < T_CYCLES_PER_FRAME) {
        
        // 1. CPU ejecuta UNA instrucción (o maneja interrupción)
        int cpu_t_cycles = global_cpu->step();
        
        // Validación de seguridad: mínimo 4 T-cycles
        if (cpu_t_cycles < 4) cpu_t_cycles = 4;
        
        // 2. PPU avanza EXACTAMENTE los mismos T-cycles
        global_ppu->step(cpu_t_cycles);
        
        // 3. Timer avanza EXACTAMENTE los mismos T-cycles
        global_timer->step(cpu_t_cycles);
        
        // 4. Acumular T-cycles del frame
        t_cycles_this_frame += cpu_t_cycles;
    }

    // Avisar a JS que dibuje cuando el frame está completo
    static int frame_count = 0;
    frame_count++;
    if (frame_count <= 3) {
        std::cout << "[MAIN LOOP] Frame #" << frame_count << " complete. Calling drawCanvas()...\n";
    }
    
    // ============================================================
    // DIAGNOSTIC: Monitor GAME_STATUS and countdown timers
    // ============================================================
    // GAME_STATUS is at 0xFF99 (HRAM[0x19])
    // COUNTDOWN is at 0xFFA6-0xFFA7 (HRAM[0x26-0x27])
    uint8_t game_status = global_mmu->readMemory(0xFF99);
    uint8_t countdown_lo = global_mmu->readMemory(0xFFA6);
    uint8_t countdown_hi = global_mmu->readMemory(0xFFA7);
    
    // State name lookup
    const char* state_name = "UNKNOWN";
    switch (game_status) {
        case 36: state_name = "MENU_COPYRIGHT_INIT"; break;
        case 37: state_name = "MENU_COPYRIGHT_1"; break;
        case 53: state_name = "MENU_COPYRIGHT_2"; break;
        case 6:  state_name = "MENU_TITLE_INIT"; break;
        case 7:  state_name = "MENU_TITLE"; break;
        case 8:  state_name = "MENU_GAME_TYPE"; break;
        case 20: state_name = "PLAYING"; break;
    }
    
    // Log state changes
    static uint8_t last_game_status = 0xFF;
    static uint8_t last_countdown = 0xFF;
    
    if (game_status != last_game_status) {
        std::cout << "\n[STATE CHANGE] Frame " << frame_count 
                  << ": GAME_STATUS = " << (int)game_status 
                  << " (" << state_name << ")\n\n";
        last_game_status = game_status;
    }
    
    // Log countdown every 60 frames (1 second) while in copyright
    if (game_status == 37 && frame_count % 60 == 0) {
        uint16_t countdown = (countdown_hi << 8) | countdown_lo;
        std::cout << "[COPYRIGHT] Frame " << frame_count 
                  << " COUNTDOWN=" << countdown
                  << " (0x" << std::hex << countdown << std::dec << ")\n";
        
        // Check if countdown changed
        if (countdown_lo != last_countdown) {
            std::cout << "  → Countdown IS changing!\n";
        } else {
            std::cout << "  → Countdown NOT changing (stuck?)\n";
        }
        last_countdown = countdown_lo;
    }
    
    // AUTO-PRESS START after 2 seconds (120 frames) to test if game advances
    static bool start_pressed = false;
    if (frame_count == 120 && !start_pressed) {
        std::cout << "\n[AUTO-TEST] Pressing START button...\n";
        std::cout << "  Current state: " << (int)game_status << " (" << state_name << ")\n\n";
        global_mmu->setButton(7, true);  // Press START
        start_pressed = true;
    }
    if (frame_count == 140 && start_pressed) {
        std::cout << "\n[AUTO-TEST] Releasing START button...\n";
        std::cout << "  Current state: " << (int)game_status << " (" << state_name << ")\n\n";
        global_mmu->setButton(7, false); // Release START
    }
    
    // Log state after START press
    if (frame_count == 180) {
        std::cout << "\n[AUTO-TEST RESULT] 1 second after START:\n";
        std::cout << "  GAME_STATUS = " << (int)game_status << " (" << state_name << ")\n";
        if (game_status == 37) {
            std::cout << "  *** STILL STUCK ON COPYRIGHT! ***\n";
        } else if (game_status == 6 || game_status == 7) {
            std::cout << "  *** SUCCESS! Game advanced to title! ***\n";
        }
        std::cout << "\n";
    }
    
    EM_ASM({
        if (typeof drawCanvas === 'function') {
            drawCanvas();
        } else {
            console.error("drawCanvas is not defined!");
        }
    });
}

int main(int argc, char **argv) {
    std::cout << "--- Game Boy Emulator Booting WASM ---" << "\n";

    std::string romPath = "/roms/tetris.gb";

    try {
        // 1. Inicializar MMU (carga la ROM)
        global_mmu = new mmu(romPath);
        
        // 2. Inicializar PPU (necesita acceso a MMU)
        const bool enable_debug = true;

        global_ppu = new ppu(*global_mmu); 
        global_ppu->enable_debug(enable_debug);

        
        // 3. Inicializar Timer (necesita acceso a MMU)
        global_timer = new timer(*global_mmu);
        global_timer->enable_debug(enable_debug);
        
        // 4. Inicializar CPU (necesita acceso a MMU)
        global_cpu = new cpu(*global_mmu); 
        
        std::cout << "Hardware inicializado correctamente.\n";
        std::cout << "Iniciando Main Loop a ~60 FPS...\n";

        // Iniciar el bucle principal
        // 0 = usar requestAnimationFrame del navegador (~60 FPS)
        // 1 = simular bucle infinito
        emscripten_set_main_loop(main_loop, 0, 1);
        
    } catch (const std::exception& e) {
        std::cout << "Error fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

// ============================================================
// NOTAS DE IMPLEMENTACIÓN:
// ============================================================
/*
IMPORTANTE: Necesitas asegurarte que:

1. CPU.step() retorna T-cycles (no M-cycles)
   - Si tu implementación actual retorna M-cycles,
     multiplica por 4 antes de pasarlo a PPU y Timer
   
   Ejemplo:
   int cpu_m_cycles = global_cpu->step();
   int cpu_t_cycles = cpu_m_cycles * 4;
   global_ppu->step(cpu_t_cycles);

2. Timer está correctamente implementado
   - Debe estar en core/cpu/timer/
   - Ver timer.h y timer.cpp que ya tienes

3. MMU tiene la función setButton()
   - Necesaria para que JS pueda actualizar botones
   - Ver mmu_FIXED.cpp para la implementación

4. El HTML debe llamar a set_button() cuando se presionen teclas
   - Ver más abajo para el código JavaScript
*/

// ============================================================
// CÓDIGO JAVASCRIPT PARA EL HTML (index.html)
// ============================================================
/*
Añade este código a tu index.html para manejar el teclado:

<script>
// Mapeo de teclas a botones de Game Boy
const keyMap = {
    'ArrowRight': 0,  // Right
    'ArrowLeft': 1,   // Left
    'ArrowUp': 2,     // Up
    'ArrowDown': 3,   // Down
    'KeyX': 4,        // A
    'KeyZ': 5,        // B
    'Shift': 6,       // Select
    'Enter': 7        // Start
};

// Manejar presión de teclas
document.addEventListener('keydown', (e) => {
    const button = keyMap[e.code];
    if (button !== undefined && Module._set_button) {
        e.preventDefault();
        Module._set_button(button, true);
    }
});

// Manejar liberación de teclas
document.addEventListener('keyup', (e) => {
    const button = keyMap[e.code];
    if (button !== undefined && Module._set_button) {
        e.preventDefault();
        Module._set_button(button, false);
    }
});

function drawCanvas() {
    if (!Module || !Module._get_video_buffer) return;

    const bufferPointer = Module._get_video_buffer();
    if (bufferPointer === 0) return;

    const data = new Uint8ClampedArray(
        Module.HEAPU8.buffer,
        bufferPointer,
        160 * 144 * 4
    );

    const canvas = document.getElementById('lcd');
    const ctx = canvas.getContext('2d');
    const imgData = new ImageData(data, 160, 144);
    ctx.putImageData(imgData, 0, 0);
}
</script>
*/