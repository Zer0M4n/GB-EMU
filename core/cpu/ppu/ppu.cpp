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
    update_stat_register(); // Actualiza el estado (HBlank, VBlank, etc.)
    scanline_counter += cycles;

    // Si hemos completado una línea (456 ciclos)...
    if (scanline_counter >= 456) {
        
        // 1. Restamos los ciclos para mantener la sincronización
        scanline_counter -= 456;

        // 2. Leemos la línea actual
        uint8_t current_line = memory.readMemory(0xFF44);

        // --- ¡MOMENTO DE DIBUJAR! ---
        // Si estamos en la zona visible (0-143), dibujamos esta línea en el buffer
        if (current_line < 144) {
            draw_scanline();
        }

        // 3. Incrementamos la línea (LY)
        memory.writeMemory(0xFF44, current_line + 1);

        // 4. Revisamos si entramos en V-BLANK (Línea 144)
        uint8_t new_line = memory.readMemory(0xFF44);
        
        if (new_line == 144) {
            // ¡Entramos en V-BLANK!
            // Activamos la interrupción V-Blank (Bit 0 del registro IF 0xFF0F)
            uint8_t if_reg = memory.readMemory(0xFF0F);
            memory.writeMemory(0xFF0F, if_reg | (1 << 0)); // Usamos bitwise OR con bit 0
        }
        else if (new_line > 153) {
            // Fin del V-Blank, volvemos a empezar el frame
            memory.writeMemory(0xFF44, 0);
        }
    }
}

void ppu::update_stat_register() {
    uint8_t current_line = memory.readMemory(0xFF44); // LY
    uint8_t status = memory.readMemory(0xFF41);       // STAT
    
    // Reseteamos solo los bits 0 y 1 (Modo), mantenemos los bits de interrupción (3-6)
    // ~0x03 es lo mismo que 11111100 en binario
    status &= ~0x03; 

    uint8_t current_mode = 0;

    // Caso 1: V-BLANK (Líneas 144 a 153)
    if (current_line >= 144) {
        current_mode = 1;      // Mode 1: V-Blank
        status |= (1 << 0);    // Ponemos bit 0 en 1
        // Nota: El bit 1 se queda en 0, así que es 01 (Modo 1)
    }
    else {
        // Estamos dibujando una línea visible (0-143)
        // Decidimos el modo según cuántos ciclos han pasado en esta línea
        
        if (scanline_counter < 80) {
            current_mode = 2;   // Mode 2: OAM Search
            status |= (1 << 1); // Ponemos bit 1 en 1 -> 10
        }
        else if (scanline_counter < (80 + 172)) { 
            current_mode = 3;   // Mode 3: Pixel Transfer
            status |= (1 << 0); 
            status |= (1 << 1); // Ponemos ambos -> 11
        }
        else {
            current_mode = 0;   // Mode 0: H-Blank
            // Ambos bits en 0 -> 00
        }
    }

    // Escribimos el nuevo estado en memoria
    memory.writeMemory(0xFF41, status);
}

void ppu::draw_scanline() {
    // 1. Obtener registros de control y scroll
    uint8_t lcdc = memory.readMemory(0xFF40);
    uint8_t scy  = memory.readMemory(0xFF42);
    uint8_t scx  = memory.readMemory(0xFF43);
    uint8_t ly   = memory.readMemory(0xFF44); // Línea actual

    // Si el LCD está apagado, no dibujamos nada
    if (!(lcdc & (1 << 7))) return;

    // 2. Determinar dónde está el mapa de tiles (Tile Map)
    // Bit 3 de LCDC decide si el mapa empieza en 0x9800 o 0x9C00
    uint16_t tile_map_base = (lcdc & (1 << 3)) ? 0x9C00 : 0x9800;

    // 3. Calcular la fila vertical dentro del mapa gigante (0-255)
    uint8_t y_pos = scy + ly;
    
    // Cada tile mide 8x8, así que dividimos por 8 para saber qué fila de tiles es
    uint16_t tile_row = (uint16_t)(y_pos / 8) * 32; 

    // 4. Bucle para los 160 píxeles horizontales
    for (int x = 0; x < 160; x++) {
        uint8_t x_pos = x + scx;

        // Determinar qué tile de la fila es (x / 8)
        uint16_t tile_col = (x_pos / 8);
        uint16_t tile_address = tile_map_base + tile_row + tile_col;

        // Leer el índice del tile (qué dibujo es)
        uint8_t tile_index = memory.readMemory(tile_address);

        // 5. Buscar los datos del tile (Tile Data)
        // Por simplicidad, asumimos el modo 0x8000 (donde los índices son 0-255)
        uint16_t tile_data_address = 0x8000 + (tile_index * 16);
        
        // Determinar qué línea interna del tile de 8x8 estamos dibujando
        uint8_t line_inside_tile = (y_pos % 8) * 2;
        uint8_t byte1 = memory.readMemory(tile_data_address + line_inside_tile);
        uint8_t byte2 = memory.readMemory(tile_data_address + line_inside_tile + 1);

        // 6. Extraer el color del píxel (0, 1, 2 o 3)
        // Hay que invertir el orden porque el bit 7 es el píxel 0
        int bit_pos = 7 - (x_pos % 8);
        uint8_t color_bit_low = (byte1 >> bit_pos) & 0x01;
        uint8_t color_bit_high = (byte2 >> bit_pos) & 0x01;
        uint8_t color_id = (color_bit_high << 1) | color_bit_low;

        // 7. Aplicar la paleta (BGP)
        uint8_t bgp = memory.readMemory(0xFF47);
        // La paleta tiene 4 colores, cada uno usa 2 bits de BGP
        uint8_t palette_index = (bgp >> (color_id * 2)) & 0x03;

        // 8. Escribir en el buffer final de 32 bits
        gfx[ly * 160 + x] = palette[palette_index];
    }
}