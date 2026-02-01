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
    IF = 0; // Asegúrate de inicializar IF también
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
        return OAM[offSet(address, 0xFE00)];
    }
    // Zona Prohibida (Unusable)
    else if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF;
    }
    
    // --- I/O Registers (0xFF00 - 0xFF7F) ---
   else if (address >= 0xFF00 && address <= 0xFF7F) {
    
    if (address == 0xFF00) {
        // Obtenemos lo que el juego escribió (bits 4 y 5 seleccionan el grupo)
        uint8_t select = IO[0x00] & 0x30; 
        
        // Retornamos: 
        // 0xC0 (bits 6 y 7 siempre en 1) 
        // | select (mantenemos la selección del juego)
        // | 0x0F (todos los botones en 1 = NO presionados)
        return 0xC0 | select | 0x0F;
    }

    return IO[offSet(address, 0xFF00)];
}
    
    // HRAM (High RAM)
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        return HRAM[offSet(address, 0xFF80)];
    }
    
    std::cout << "Memory Read Error: Address not mapped " << std::hex << address << "\n";
    return 0xFF;
}

// --- Escritura de Memoria ---
void mmu::writeMemory(uint16_t address, uint8_t value) 
{
    // Interrupciones y DMA (Interceptar antes)
    if (address == 0xFF46) { DMA(value); return; }
    if (address == 0xFF0F) { IF = value; return; }
    if (address == 0xFFFF) { IE = value; return; }

    // Zona Prohibida (Ignorar)
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return; 
    }

    // ROM (Banking en el cartucho)
    if (address <= 0x7FFF) {
        cart.writeCartridge(address, value);
        return; // IMPORTANTE: Return para que no siga bajando
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
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
        OAM[offSet(address, 0xFE00)] = value;
        return;
    }
    // I/O Registers (AQUI ENTRA EL 0xFF41 STAT)
    else if (address >= 0xFF00 && address <= 0xFF7F) {
        IO[offSet(address, 0xFF00)] = value;
        return;
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        HRAM[offSet(address, 0xFF80)] = value;
        return;
    }
    
    // Si llega aquí, es un error real
    std::cout << "Memory WRITE Error: Address not mapped " << std::hex << address << "\n";
}