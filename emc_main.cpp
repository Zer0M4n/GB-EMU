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
// MAIN LOOP - MEJORADO CON TIMER
// ============================================================
void main_loop() {
    if (!global_cpu || !global_ppu || !global_timer) return;

    // TIMING CORRECTO
    // La Game Boy funciona a 4,194,304 Hz.
    // 70224 dots por frame = 17556 M-cycles
    // A 60 FPS, cada frame debe tomar ~16.74ms
    const int DOTS_PER_FRAME = 70224;
    const int M_CYCLES_PER_FRAME = 17556; // 70224 / 4
    
    int cycles_this_frame = 0;

    // Ejecutar hasta completar un frame
    while (cycles_this_frame < M_CYCLES_PER_FRAME) {
        
        // 1. CPU ejecuta UNA instrucción
        //    step() ahora retorna T-cycles (4 T-cycles = 1 M-cycle)
        int cpu_t_cycles = global_cpu->step();
        int cpu_m_cycles = cpu_t_cycles / 4; // Convertir a M-cycles si es necesario
        
        // 2. PPU avanza el mismo tiempo
        //    step() ahora recibe T-cycles directamente
        global_ppu->step(cpu_t_cycles);
        
        // 3. TIMER avanza el mismo tiempo
        //    step() recibe T-cycles
        global_timer->step(cpu_t_cycles);
        
        // 4. Acumular ciclos del frame
        //    Usamos M-cycles para contar
        cycles_this_frame += cpu_m_cycles;
    }

    // Avisar a JS que dibuje cuando el frame está completo
    EM_ASM({
        if (typeof drawCanvas === 'function') {
            drawCanvas();
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
        global_ppu = new ppu(*global_mmu); 
                global_ppu->enable_debug(true);  // ← AQUÍ: Habilitar debugger de scanline

        
        // 3. Inicializar Timer (necesita acceso a MMU)
        global_timer = new timer(*global_mmu);
        
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