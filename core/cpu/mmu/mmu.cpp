// ============================================================
// MMU.CPP - SECCIÓN CORREGIDA PARA JOYPAD
// ============================================================
// Reemplaza la función readMemory en tu mmu.cpp

#include "mmu.h"
#include <iostream>

// --- Constructor ---
mmu::mmu(const std::string& romPath) : cart(romPath) 
{
    VRAM.fill(0);
    WRAM.fill(0);
    HRAM.fill(0);
    IO.fill(0);
    OAM.fill(0);
    IE = 0;
    IF = 0;
    
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
    for (size_t i = 0; i < OAM.size(); i++) 
    {
        OAM[i] = readMemory(base + static_cast<uint16_t>(i));
    }
}

// ============================================================
// READ MEMORY - CORREGIDA CON JOYPAD ARREGLADO
// ============================================================
uint8_t mmu::readMemory(uint16_t address) 
{   
    // Interrupciones (Registro IF e IE)
    if (address == 0xFF0F) return IF;
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
        // JOYPAD (0xFF00) - IMPLEMENTACIÓN CORRECTA
        // ============================================================
        if (address == 0xFF00) {
            // Leer el registro P1/JOYP
            uint8_t p1 = IO[0x00];
            
            // Bits 6-7 siempre están en 1 (no se usan)
            uint8_t result = 0xC0;
            
            // Mantener bits 4-5 (selección de grupo)
            result |= (p1 & 0x30);
            
            // IMPORTANTE: Por defecto, todos los botones NO presionados (bits en 1)
            uint8_t button_state = 0x0F;
            
            // TODO: Aquí deberías verificar el estado real de los botones
            // Por ahora, simulamos que NO hay botones presionados
            
            bool select_dpad = !(p1 & 0x10);    // Bit 4 = 0 → D-Pad seleccionado
            bool select_buttons = !(p1 & 0x20); // Bit 5 = 0 → Botones seleccionados
            
            // Ejemplo de cómo leerías botones reales:
            /*
            if (select_dpad) {
                if (button_right_pressed)  button_state &= ~0x01;
                if (button_left_pressed)   button_state &= ~0x02;
                if (button_up_pressed)     button_state &= ~0x04;
                if (button_down_pressed)   button_state &= ~0x08;
            }
            
            if (select_buttons) {
                if (button_a_pressed)      button_state &= ~0x01;
                if (button_b_pressed)      button_state &= ~0x02;
                if (button_select_pressed) button_state &= ~0x04;
                if (button_start_pressed)  button_state &= ~0x08;
            }
            */
            
            // Combinar resultado final
            result |= button_state;
            
            return result;
        }
        
        // Otros registros I/O
        return IO[offSet(address, 0xFF00)];
    }
    
    // HRAM (High RAM)
    else if (address >= 0xFF80 && address <= 0xFFFE) {
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
    if (address == 0xFF0F) { IF = value & 0x1F; return; } // Solo bits 0-4
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
            // Solo bits 4 y 5 son escribibles
            // Mantener bits 0-3 (estado de botones) y bits 6-7 (siempre 1)
            IO[0x00] = (value & 0x30) | (IO[0x00] & 0xCF);
            return;
        }
        
        // Escritura especial para DIV (0xFF04)
        if (address == 0xFF04) {
            // Escribir CUALQUIER valor a DIV lo resetea a 0
            IO[0x04] = 0;
            return;
        }
        
        // Otros registros I/O normales
        IO[offSet(address, 0xFF00)] = value;
        return;
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        HRAM[offSet(address, 0xFF80)] = value;
        return;
    }
    
    std::cout << "Memory WRITE Error: Address not mapped " << std::hex << address << "\n";
}

// ============================================================
// FUNCIÓN PARA ACTUALIZAR ESTADO DE BOTONES (NUEVA)
// ============================================================
// Añade esta función al mmu.h y mmu.cpp para permitir
// que JavaScript actualice el estado de los botones

/*
// En mmu.h, añadir:
private:
    // Estado de los botones (false = no presionado, true = presionado)
    bool button_right = false;
    bool button_left = false;
    bool button_up = false;
    bool button_down = false;
    bool button_a = false;
    bool button_b = false;
    bool button_select = false;
    bool button_start = false;

public:
    void setButton(int button_id, bool pressed);
    
// Luego implementar en mmu.cpp:
void mmu::setButton(int button_id, bool pressed) {
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
}

// Y usar en readMemory (0xFF00):
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
*/