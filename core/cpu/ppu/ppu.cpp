#include "ppu.h"

ppu::ppu(mmu& mmu_ref) : memory(mmu_ref)
{
    scanline_counter = 0;
    frame_complete = false;
    
    // Inicializamos buffer gráfico (160 * 144 píxeles)
    gfx.resize(160 * 144);
    std::fill(gfx.begin(), gfx.end(), 0xFF0FBC9B); // Color base verde claro

    // Paleta clásica "Game Boy" (Formato 0xAARRGGBB o similar según tu frontend)
    // 0: Blanco/Verde claro, 3: Negro/Verde oscuro
    palette[0] = 0xFF0FBC9B; 
    palette[1] = 0xFF0FAC8B; 
    palette[2] = 0xFF306230; 
    palette[3] = 0xFF0F380F; 
}

void ppu::step(int cycles) {
    // 1. Actualizamos el estado (STAT) antes de procesar
    //    Esto decide en qué Modo estamos (HBlank, VBlank, etc.)
    update_stat_register();

    // 2. Comprobar si el LCD está ENCENDIDO (Bit 7 de LCDC - 0xFF40)
    uint8_t lcdc = memory.readMemory(0xFF40);
    if (!(lcdc & (1 << 7))) {
        // --- CASO LCD APAGADO ---
        // Si el LCD se apaga, reseteamos contadores y ponemos la PPU en "reposo"
        scanline_counter = 0;
        memory.writeMemory(0xFF44, 0); // LY (Línea actual) a 0
        
        // Forzamos el modo a VBlank (Mode 1) en el registro STAT
        // para evitar que los juegos se bloqueen esperando modos.
        uint8_t status = memory.readMemory(0xFF41);
        status &= ~0x03; // Borrar bits 0-1
        status |= 0x01;  // Poner modo 1
        memory.writeMemory(0xFF41, status);
        return;
    }

    // --- CASO LCD ENCENDIDO ---
    scanline_counter += cycles;

    // Una línea de escaneo tarda 456 ciclos de CPU (Dots)
    if (scanline_counter >= 456) {
        // Restamos 456 para mantener el remanente de ciclos para la siguiente línea
        scanline_counter -= 456;

        // Leemos la línea actual (LY)
        uint8_t current_line = memory.readMemory(0xFF44);

        // A. Si la línea está dentro de la pantalla visible (0 - 143)
        if (current_line < 144) {
            draw_scanline(); // ¡Dibujamos píxeles en el buffer!
        }

        // B. Pasamos a la siguiente línea
        current_line++;
        memory.writeMemory(0xFF44, current_line);

        // C. Verificar eventos por número de línea
        if (current_line == 144) {
            // --- INICIO DE V-BLANK ---
            // Acabamos de dibujar toda la pantalla.
            
            // 1. Solicitamos Interrupción V-Blank (Bit 0 de IF - 0xFF0F)
            uint8_t if_reg = memory.readMemory(0xFF0F);
            memory.writeMemory(0xFF0F, if_reg | (1 << 0));
            
            // 2. Avisamos al frontend que puede pintar el frame
            frame_complete = true;
        }
        else if (current_line > 153) {
            // --- FIN DE V-BLANK ---
            // La PPU ha terminado las líneas invisibles (144-153).
            // Volvemos a la línea 0 para empezar un nuevo frame.
            memory.writeMemory(0xFF44, 0);
            frame_complete = false;
        }
    }
}

// =============================================================
// FUNCIÓN UPDATE_STAT: Máquina de estados
// Controla los modos (0,1,2,3) y las interrupciones LCD
// =============================================================
void ppu::update_stat_register() {
    // Si el LCD está apagado, no actualizamos nada
    if (!(memory.readMemory(0xFF40) & (1 << 7))) return;

    uint8_t current_line = memory.readMemory(0xFF44); // LY
    uint8_t status = memory.readMemory(0xFF41);       // Leemos STAT actual

    // Guardamos el modo actual (bits 0 y 1) para detectar cambios
    uint8_t current_mode = status & 0x03;
    uint8_t new_mode = 0;
    bool req_interrupt = false;

    // --- 1. DETERMINAR EL NUEVO MODO ---
    if (current_line >= 144) {
        new_mode = 1; // V-BLANK (Líneas 144-153)
        // Bit 4 de STAT: Interrupción VBlank activada en STAT
        if (status & (1 << 4)) req_interrupt = true;
    }
    else {
        // Líneas visibles (0-143)
        if (scanline_counter < 80) {
            new_mode = 2; // OAM SEARCH (Buscando sprites)
            // Bit 5 de STAT: Interrupción OAM activada
            if (status & (1 << 5)) req_interrupt = true;
        }
        else if (scanline_counter < (80 + 172)) {
            new_mode = 3; // PIXEL TRANSFER (Dibujando)
            // El modo 3 no tiene interrupción STAT
        }
        else {
            new_mode = 0; // H-BLANK (Descanso horizontal)
            // Bit 3 de STAT: Interrupción HBlank activada
            if (status & (1 << 3)) req_interrupt = true;
        }
    }

    // --- 2. LOGICA DE INTERRUPCIÓN STAT ---
    // La interrupción se dispara solo cuando CAMBIAMOS de modo
    if (req_interrupt && (current_mode != new_mode)) {
        uint8_t if_reg = memory.readMemory(0xFF0F);
        memory.writeMemory(0xFF0F, if_reg | (1 << 1)); // Bit 1: LCD STAT Interrupt
    }

    // --- 3. CHECK LY == LYC (Coincidencia) ---
    uint8_t lyc = memory.readMemory(0xFF45);
    if (current_line == lyc) {
        status |= (1 << 2); // Encender bit 2 (Coincidence Flag)
        
        // Si la interrupción de coincidencia está activada (Bit 6)
        if (status & (1 << 6)) {
             uint8_t if_reg = memory.readMemory(0xFF0F);
             memory.writeMemory(0xFF0F, if_reg | (1 << 1));
        }
    } else {
        status &= ~(1 << 2); // Apagar bit 2
    }

    // --- 4. GUARDAR ESTADO ---
    // Limpiamos los bits de modo antiguos (bits 0-1) y ponemos el nuevo
    status &= ~0x03; 
    status |= new_mode;
    
    memory.writeMemory(0xFF41, status);
}

void ppu::draw_scanline() {
    uint8_t lcdc = memory.readMemory(0xFF40);
    uint8_t scy = memory.readMemory(0xFF42);
    uint8_t scx = memory.readMemory(0xFF43);
    uint8_t ly = memory.readMemory(0xFF44);
    uint8_t bgp = memory.readMemory(0xFF47); // Paleta Fondo

    // Verificar si el Fondo está activado (Bit 0 LCDC)
    if (!(lcdc & 1)) return; 

    // Mapa de Tiles: ¿0x9C00 o 0x9800? (Bit 3)
    uint16_t tile_map_base = (lcdc & (1 << 3)) ? 0x9C00 : 0x9800;

    // Datos de Tiles: ¿0x8000 o 0x8800? (Bit 4)
    // 1 = 8000-8FFF (Unsigned 0-255)
    // 0 = 8800-97FF (Signed -128 a 127)
    bool signed_tile_addressing = !(lcdc & (1 << 4));

    uint8_t y_pos = scy + ly;
    uint16_t tile_row = (y_pos / 8) * 32;

    for (int x = 0; x < 160; x++) {
        uint8_t x_pos = x + scx;
        
        // Qué tile es (columna)
        uint16_t tile_col = (x_pos / 8);
        uint16_t tile_addr = tile_map_base + tile_row + tile_col;
        
        // Indice del tile
        uint8_t tile_num = memory.readMemory(tile_addr);
        
        // CALCULAR DIRECCIÓN DEL TILE DATA (La corrección importante)
        uint16_t tile_location = 0;
        if (signed_tile_addressing) {
            // Modo 0: 0x9000 base + indice con signo
            tile_location = 0x9000 + ((int8_t)tile_num * 16);
        } else {
            // Modo 1: 0x8000 base + indice sin signo
            tile_location = 0x8000 + (tile_num * 16);
        }

        // Qué línea vertical del tile (0-7) * 2 bytes
        uint8_t line = (y_pos % 8) * 2;
        
        uint8_t data1 = memory.readMemory(tile_location + line);
        uint8_t data2 = memory.readMemory(tile_location + line + 1);

        // Bit del color (invertido, bit 7 es pixel 0)
        int color_bit = 7 - (x_pos % 8);
        
        // Combinar bits
        int color_num = ((data2 >> color_bit) & 1) << 1;
        color_num |= ((data1 >> color_bit) & 1);

        // Mapear con paleta (BGP)
        int color = (bgp >> (color_num * 2)) & 0x03;

        // Escribir en buffer
        gfx[ly * 160 + x] = palette[color];
    }
}