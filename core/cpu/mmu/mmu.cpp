#include "mmu.h"
#include <iostream>

// --- Constructor ---
mmu::mmu(const std::string& romPath) : cart(romPath) 
{
    // Inicializar memorias a 0
    VRAM.fill(0);
    WRAM.fill(0);
    HRAM.fill(0);
    IO.fill(0);
    OAM.fill(0);
    IE = 0;
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
    for (size_t i = 0; i < OAM.size(); i++) // 'size_t' para evitar warning
    {
        OAM[i] = readMemory(base + static_cast<uint16_t>(i));
    }
}

// --- Lectura de Memoria ---
uint8_t mmu::readMemory(uint16_t address) 
{   
    if (address == 0xFFFF) {
        return IE;
    }
    
    // Cartucho (ROM)
    if (address <= 0x7FFF) {   
        return cart.readCartridge(address);
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
        return VRAM[offSet(address, 0x8000)];
    }
    // Cartucho (RAM externa)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        return cart.readCartridge(address);
    }
    // WRAM
    else if (address >= 0xC000 && address <= 0xDFFF) {
        return WRAM[offSet(address, 0xC000)];
    }
    // ECHO RAM (Espejo de WRAM)
    else if (address >= 0xE000 && address <= 0xFDFF) {
        return WRAM[offSet(address, 0xE000)];
    }
    // OAM
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        return OAM[offSet(address, 0xFE00)];
    }
    // I/O Registers
    else if (address >= 0xFF00 && address <= 0xFF46) { // Nota: El rango de IO es mayor, pero seguimos tu logica
        return IO[offSet(address, 0xFF00)];
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        return HRAM[offSet(address, 0xFF80)];
    }
    else {
        // std::cout << "Memory Read Error: Address not mapped " << std::hex << address << "\n";
        return 0xFF; // Retornamos 0xFF en bus abierto (mejor que NULL)
    }
}

// --- Escritura de Memoria ---
void mmu::writeMemory(uint16_t address, uint8_t value) 
{
    // Registro DMA
    if (address == 0xFF46) {
        DMA(value);
        return;
    }
    // Interrupt Enable
    if (address == 0xFFFF) {
        IE = value;
        return;
    }

    // Cartucho (ROM - Banking)
    if (address <= 0x7FFF) {
        cart.writeCartridge(address, value);
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
        VRAM[offSet(address, 0x8000)] = value;
    }
    // Cartucho (RAM)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        cart.writeCartridge(address, value);
    }
    // WRAM
    else if (address >= 0xC000 && address <= 0xDFFF) {
        WRAM[offSet(address, 0xC000)] = value;
    }
    // OAM
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        OAM[offSet(address, 0xFE00)] = value;
    }
    // I/O Registers
    else if (address >= 0xFF00 && address < 0xFF80) { // Ajustado para cubrir IO
        IO[offSet(address, 0xFF00)] = value;
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        HRAM[offSet(address, 0xFF80)] = value;
    }
    else {
        // std::cout << "Memory Write Error: Address not mapped\n";
    }
}