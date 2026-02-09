// ============================================================
// MMU.CPP - SECCIÓN CORREGIDA PARA JOYPAD
// ============================================================
// Reemplaza la función readMemory en tu mmu.cpp

#include "mmu.h"
#include <iostream>
#include <iomanip>

// --- Constructor ---
mmu::mmu(const std::string& romPath) : cart(romPath) 
{
    VRAM.fill(0);
    WRAM.fill(0);
    HRAM.fill(0);
    IO.fill(0);
    OAM.fill(0);
    IE = 0;
    // IF ahora usa IO[0x0F] directamente para evitar desincronización
    IO[0x0F] = 0; // IF - Interrupt Flag
    
    // IMPORTANTE: Inicializar el joypad correctamente
    // Bits 4-5 deben estar en 1 por defecto (ningún grupo seleccionado)
    IO[0x00] = 0xFF; // 0xFF00 - Joypad
    
    std::cout << "MMU Inicializada. Cartucho conectado: " << romPath << "\n";
}

// --- Helper: Calcular Offset ---
uint16_t mmu::offSet(uint16_t address, uint16_t base)
{
    return address - base;
}

// --- Helper: DMA Transfer ---
void mmu::DMA(uint8_t value) 
{
    uint16_t base = value << 8;
    
    // DEBUG: Log DMA transfers
    static int dma_count = 0;
    dma_count++;
    if (dma_count <= 10 || dma_count == 100 || dma_count == 500) {
        std::cout << "[DMA #" << dma_count << "] Transfer from 0x" 
                  << std::hex << base << std::dec << " to OAM\n";
        
        // Sample source data before copy
        std::cout << "  Source first 8 bytes: ";
        for (int i = 0; i < 8; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)readMemory(base + i) << " ";
        }
        std::cout << std::dec << "\n";
    }
    
    for (size_t i = 0; i < OAM.size(); i++) 
    {
        OAM[i] = readMemory(base + static_cast<uint16_t>(i));
    }
    
    // After copy, check if any sprites are non-zero
    if (dma_count <= 10) {
        int non_zero = 0;
        for (size_t i = 0; i < OAM.size(); i += 4) {
            if (OAM[i] != 0 || OAM[i+1] != 0) non_zero++;
        }
        std::cout << "  After DMA: " << non_zero << "/40 non-zero sprites in OAM\n";
    }
}

// ============================================================
// READ MEMORY - CORREGIDA CON JOYPAD ARREGLADO
// ============================================================
uint8_t mmu::readMemory(uint16_t address) 
{   
    // Interrupciones (Registro IF e IE)
    // IF usa IO[0x0F] directamente - bits 5-7 siempre retornan 1
    if (address == 0xFF0F) {
        uint8_t result = IO[0x0F] | 0xE0;
        // DEBUG: Log cuando IF tiene VBlank activo
        static int if_read_vblank_count = 0;
        if ((IO[0x0F] & 0x01) && if_read_vblank_count < 20) {
            if_read_vblank_count++;
            std::cout << "[MMU] IF read while VBlank active! Returning 0x" 
                      << std::hex << (int)result << std::dec << "\n";
        }
        return result;
    }
    if (address == 0xFFFF) return IE;
    
    // ROM (Cartucho)
    if (address <= 0x7FFF) {   
        return cart.readCartridge(address);
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
        // TODO: Verificar si PPU está en modo 3 (Drawing)
        // Durante modo 3, VRAM no es accesible
        return VRAM[offSet(address, 0x8000)];
    }
    // RAM Externa (Cartucho)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        return cart.readCartridge(address);
    }
    // WRAM (Work RAM)
    else if (address >= 0xC000 && address <= 0xDFFF) {
        return WRAM[offSet(address, 0xC000)];
    }
    // ECHO RAM (Espejo de WRAM)
    else if (address >= 0xE000 && address <= 0xFDFF) {
        return WRAM[offSet(address, 0xE000)];
    }
    // OAM (Object Attribute Memory)
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        // TODO: Verificar si PPU está en modo 2 o 3
        // Durante modos 2 y 3, OAM no es accesible
        return OAM[offSet(address, 0xFE00)];
    }
    // Zona Prohibida (Unusable)
    else if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF;
    }
    
    // --- I/O Registers (0xFF00 - 0xFF7F) ---
    else if (address >= 0xFF00 && address <= 0xFF7F) {
        
        // ============================================================
        // HARD HOOK: LY (0xFF44) - DEBUG CRÍTICO
        // ============================================================
        if (address == 0xFF44) {
            uint8_t val = IO[0x44];
            
            // DEBUG: Log para confirmar qué ve la CPU
            static int ly_read_count = 0;
            static uint8_t last_ly = 0xFF;
            ly_read_count++;
            
            // Log cuando LY cambia o primeras lecturas
            if (val != last_ly || ly_read_count <= 10) {
                if (ly_read_count <= 50 || val == 144) {
                    std::cout << "[MMU DEBUG] Read LY (FF44) returned: " << std::dec << (int)val
                              << " (read #" << ly_read_count << ")\n";
                }
                last_ly = val;
            }
            
            // Log especial cuando LY llega a 144
            static bool logged_144 = false;
            if (val == 144 && !logged_144) {
                std::cout << "[MMU DEBUG] *** LY=144 VISIBLE TO CPU! ***\n";
                logged_144 = true;
            }
            
            return val;
        }
        
        // ============================================================
        // JOYPAD (0xFF00) - ACTIVE LOW MATRIX LOGIC
        // ============================================================
        if (address == 0xFF00) {
            uint8_t p1 = IO[0x00];
            
            // Bits 6-7 siempre en 1 (no se usan)
            uint8_t result = 0xC0;
            
            // Mantener bits 4-5 (selección de grupo)
            result |= (p1 & 0x30);
            
            // Por defecto, todos los botones NO presionados (bits en 1 = Active Low)
            uint8_t button_state = 0x0F;
            
            bool select_dpad = !(p1 & 0x10);    // Bit 4 = 0 → D-Pad seleccionado
            bool select_buttons = !(p1 & 0x20); // Bit 5 = 0 → Botones seleccionados
            
            // Matriz de botones (Active Low: 0 = presionado)
            if (select_dpad) {
                if (button_right)  button_state &= ~0x01;
                if (button_left)   button_state &= ~0x02;
                if (button_up)     button_state &= ~0x04;
                if (button_down)   button_state &= ~0x08;
            }
            
            if (select_buttons) {
                if (button_a)      button_state &= ~0x01;
                if (button_b)      button_state &= ~0x02;
                if (button_select) button_state &= ~0x04;
                if (button_start)  button_state &= ~0x08;
            }
            
            result |= button_state;
            
            // DEBUG: Log cuando hay botones presionados (especially START)
            static int joypad_read_count = 0;
            static int start_detected_count = 0;
            joypad_read_count++;
            
            // Log if START is pressed and buttons are being read
            if (button_start && select_buttons) {
                start_detected_count++;
                if (start_detected_count <= 20) {
                    std::cout << "[JOYPAD START!] Read #" << start_detected_count
                              << " P1=0x" << std::hex << (int)result
                              << " btn_state=0x" << (int)button_state
                              << " (START should clear bit 3)" << std::dec << "\n";
                }
            }
            
            // Also log if START is pressed but wrong button group selected
            if (button_start && !select_buttons && joypad_read_count <= 200) {
                std::cout << "[JOYPAD MISS] START pressed but dpad selected, result=0x" 
                          << std::hex << (int)result << std::dec << "\n";
            }
            
            if (button_state != 0x0F && joypad_read_count <= 50) {
                std::cout << "[JOYPAD] Read P1=0x" << std::hex << (int)result
                          << " buttons=" << (int)button_state
                          << " sel_dpad=" << select_dpad
                          << " sel_btn=" << select_buttons
                          << std::dec << "\n";
            }
            
            // Log periódico para ver si el juego lee joypad
            if (joypad_read_count == 1000) {
                std::cout << "[JOYPAD] 1000 reads so far. Game IS polling joypad.\n";
            }
            
            return result;
        }
        
        // Otros registros I/O
        return IO[offSet(address, 0xFF00)];
    }
    
    // HRAM (High RAM)
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        // DEBUG: Track reads from 0xFF85 (heavily polled in Tetris)
        if (address == 0xFF85) {
            static int ff85_read_count = 0;
            ff85_read_count++;
            if (ff85_read_count <= 10 || ff85_read_count == 1000 || ff85_read_count == 10000) {
                std::cout << "[HRAM 0xFF85] Read #" << ff85_read_count 
                          << " value=0x" << std::hex << (int)HRAM[0x05] << std::dec << "\n";
            }
        }
        return HRAM[offSet(address, 0xFF80)];
    }
    
    std::cout << "Memory Read Error: Address not mapped " << std::hex << address << "\n";
    return 0xFF;
}

// ============================================================
// WRITE MEMORY - CORREGIDA
// ============================================================
void mmu::writeMemory(uint16_t address, uint8_t value) 
{
    // Interrupciones y DMA (Interceptar antes)
    if (address == 0xFF46) { DMA(value); return; }
    // IF usa IO[0x0F] directamente - SEMÁNTICA ESPECIAL
    // En Game Boy real:
    // - Hardware (PPU/Timer) PONE bits usando OR
    // - Software LIMPIA bits escribiendo 1s (acknowledge)
    // Pero muchos juegos simplemente sobrescriben, así que usamos escritura directa
    if (address == 0xFF0F) { 
        // DEBUG: Rastrear escrituras a IF
        static int if_write_count = 0;
        if_write_count++;
        if (if_write_count <= 20) {
            std::cout << "[MMU IF WRITE #" << if_write_count 
                      << "] value=0x" << std::hex << (int)value
                      << " IO[0x0F] was 0x" << (int)IO[0x0F]
                      << std::dec << "\n";
        }
        // Solo bits 0-4 son válidos
        IO[0x0F] = value & 0x1F; 
        return; 
    }
    if (address == 0xFFFF) { IE = value & 0x1F; return; } // Solo bits 0-4

    // Zona Prohibida (Ignorar)
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return; 
    }

    // ROM (Banking en el cartucho)
    if (address <= 0x7FFF) {
        cart.writeCartridge(address, value);
        return;
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
        // TODO: Verificar si PPU está en modo 3 (Drawing)
        // Durante modo 3, ignorar escrituras
        VRAM[offSet(address, 0x8000)] = value;
        return;
    }
    // RAM Externa
    else if (address >= 0xA000 && address <= 0xBFFF) {
        cart.writeCartridge(address, value);
        return;
    }
    // WRAM
    else if (address >= 0xC000 && address <= 0xDFFF) {
        // DEBUG: Track writes to sprite buffer area (0xC000-0xC09F)
        static int sprite_buffer_writes = 0;
        if (address >= 0xC000 && address <= 0xC09F && value != 0) {
            sprite_buffer_writes++;
            if (sprite_buffer_writes <= 20) {
                std::cout << "[WRAM SPRITE BUFFER] Write #" << sprite_buffer_writes
                          << " to 0x" << std::hex << address 
                          << " = 0x" << (int)value << std::dec << "\n";
            }
            if (sprite_buffer_writes == 100) {
                std::cout << "[WRAM SPRITE BUFFER] 100 writes to sprite buffer area!\n";
            }
        }
        WRAM[offSet(address, 0xC000)] = value;
        return;
    }
    // ECHO RAM (Escribir en WRAM correspondiente)
    else if (address >= 0xE000 && address <= 0xFDFF) {
        WRAM[offSet(address, 0xE000)] = value;
        return;
    }
    // OAM
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        // TODO: Verificar si PPU está en modo 2 o 3
        // Durante modos 2 y 3, ignorar escrituras
        OAM[offSet(address, 0xFE00)] = value;
        return;
    }
    // I/O Registers
    else if (address >= 0xFF00 && address <= 0xFF7F) {
        // ============================================================
        // JOYPAD (0xFF00) - ESCRITURA CORRECTA
        // ============================================================
        if (address == 0xFF00) {
            // DEBUG: Track P1 writes
            static int p1_write_count = 0;
            p1_write_count++;
            if (p1_write_count <= 20) {
                std::cout << "[JOYPAD WRITE #" << p1_write_count << "] value=0x" 
                          << std::hex << (int)value 
                          << " (select_dpad=" << !(value & 0x10)
                          << " select_btn=" << !(value & 0x20)
                          << ")" << std::dec << "\n";
            }
            
            // Solo bits 4 y 5 son escribibles
            // Mantener bits 0-3 (estado de botones) y bits 6-7 (siempre 1)
            IO[0x00] = (value & 0x30) | (IO[0x00] & 0xCF);
            return;
        }
        
        // Escritura especial para DIV (0xFF04)
        if (address == 0xFF04) {
            // Escribir CUALQUIER valor a DIV lo resetea a 0
            static int div_reset_count = 0;
            div_reset_count++;
            if (div_reset_count <= 20) {
                std::cout << "[MMU] DIV (0xFF04) reset to 0! (write #" << div_reset_count 
                          << ", was 0x" << std::hex << (int)IO[0x04] << ")\n" << std::dec;
            }
            IO[0x04] = 0;
            return;
        }
        
        // Otros registros I/O normales
        IO[offSet(address, 0xFF00)] = value;
        return;
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        // DEBUG: Track writes to 0xFF85 (VBLANK_DONE)
        if (address == 0xFF85) {
            static int ff85_write_count = 0;
            ff85_write_count++;
            if (ff85_write_count <= 20) {
                std::cout << "[HRAM 0xFF85] Write #" << ff85_write_count 
                          << " value=0x" << std::hex << (int)value << std::dec << "\n";
            }
        }
        
        // DEBUG: Track writes to 0xFF99 (GAME_STATUS) - CRITICAL for state machine
        if (address == 0xFF99) {
            static int game_status_write_count = 0;
            game_status_write_count++;
            
            const char* state_name = "UNKNOWN";
            switch (value) {
                case 36: state_name = "MENU_COPYRIGHT_INIT"; break;
                case 37: state_name = "MENU_COPYRIGHT_1"; break;
                case 53: state_name = "MENU_COPYRIGHT_2"; break;
                case 6:  state_name = "MENU_TITLE_INIT"; break;
                case 7:  state_name = "MENU_TITLE"; break;
                case 8:  state_name = "MENU_GAME_TYPE"; break;
                case 20: state_name = "PLAYING"; break;
            }
            
            std::cout << "[GAME_STATUS WRITE #" << game_status_write_count 
                      << "] value=" << std::dec << (int)value 
                      << " (" << state_name << ")" 
                      << " addr=0x" << std::hex << address << std::dec << "\n";
        }
        
        HRAM[offSet(address, 0xFF80)] = value;
        return;
    }
    
    std::cout << "Memory WRITE Error: Address not mapped " << std::hex << address << "\n";
}

// ============================================================
// FUNCIÓN PARA ACTUALIZAR ESTADO DE BOTONES
// ============================================================
// Button IDs: 0=Right, 1=Left, 2=Up, 3=Down, 4=A, 5=B, 6=Select, 7=Start
// ============================================================
void mmu::setButton(int button_id, bool pressed) {
    const char* button_names[] = {"RIGHT", "LEFT", "UP", "DOWN", "A", "B", "SELECT", "START"};
    
    if (button_id >= 0 && button_id <= 7) {
        std::cout << "[JOYPAD_BACKEND] setButton(" << button_names[button_id] 
                  << ", " << (pressed ? "PRESSED" : "RELEASED") << ")\n";
    }
    
    switch(button_id) {
        case 0: button_right = pressed; break;
        case 1: button_left = pressed; break;
        case 2: button_up = pressed; break;
        case 3: button_down = pressed; break;
        case 4: button_a = pressed; break;
        case 5: button_b = pressed; break;
        case 6: button_select = pressed; break;
        case 7: button_start = pressed; break;
    }
    
    // Opcionalmente, solicitar interrupción de Joypad (bit 4 de IF)
    // Esto despierta a la CPU si está en HALT esperando input
    if (pressed) {
        IO[0x0F] |= 0x10;  // Set Joypad interrupt flag
    }
}