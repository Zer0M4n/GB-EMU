#include "ppu.h"

// Direcciones de registros importantes (para referencia)
// LCDC = 0xFF40
// STAT = 0xFF41
// SCY  = 0xFF42
// SCX  = 0xFF43
// LY   = 0xFF44 

ppu::ppu(mmu& mmu_ref) : memory(mmu_ref)
{
    // 1. Inicializamos contadores
    scanline_counter = 0;

    // 2. Limpiamos la pantalla (llenar de blanco/verde claro)
    gfx.fill(0xFF0FBC9B); 

    // 3. Cargamos la Paleta "Matrix Green" (0xAABBGGRR)
    palette[0] = 0xFF0FBC9B; // Blanco
    palette[1] = 0xFF0FAC8B; // Gris Claro
    palette[2] = 0xFF306230; // Gris Oscuro
    palette[3] = 0xFF0F380F; // Negro
}
void ppu::step(int cycles) {
    update_stat_register(); // Actualizar estados (más adelante)
    
    scanline_counter += cycles;

    // Aquí irá la lógica de la máquina de estados
    // que decide cuándo dibujar una línea.
}

void ppu::update_stat_register() {
    // Aquí gestionaremos los modos (HBlank, VBlank...)
}

void ppu::draw_scanline() {
    // Aquí leeremos la VRAM y pintaremos en 'gfx'
}
