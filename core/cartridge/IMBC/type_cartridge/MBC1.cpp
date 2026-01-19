#include "MBC1.h"

MBC1::MBC1(std::vector<uint8_t>& rom_ref, std::vector<uint8_t>& ram_ref, uint16_t banks) 
    : rom(rom_ref), ram(ram_ref), romBanksCount(banks)
{
    romBank = 1; // El banco 1 es el default en 0x4000-0x7FFF
    ramBank = 0;
    ramEnabled = false;
    bankingMode = 0;
}

uint8_t MBC1::readROM(uint16_t address) {
    // Banco 0 (Fijo)
    if (address < 0x4000) {
        return rom[address];
    }
    // Banco N (Switchable)
    if (address >= 0x4000 && address <= 0x7FFF) {
        uint32_t target = (romBank * 0x4000) + (address - 0x4000);
        // Evitar desbordamiento
        if (target < rom.size()) return rom[target];
    }
    return 0xFF;
}

void MBC1::writeROM(uint16_t address, uint8_t value) {
    // 1. RAM Enable
    if (address < 0x2000) {
        ramEnabled = ((value & 0x0F) == 0x0A);
    }
    // 2. ROM Bank Number
    else if (address >= 0x2000 && address < 0x4000) {
        romBank = value & 0x1F; 
        if (romBank == 0) romBank = 1; // MBC1 traduce 0 a 1
    }
    // 3. RAM Bank Number / Upper Bits ROM
    else if (address >= 0x4000 && address < 0x6000) {
        ramBank = value & 0x03;
    }
    // 4. Banking Mode Select
    else if (address >= 0x6000 && address < 0x8000) {
        bankingMode = value & 0x01;
    }
}

uint8_t MBC1::readRAM(uint16_t address) {
    if (!ramEnabled) return 0xFF;
    
    // Simplificado para RAM básica
    uint16_t offset = address - 0xA000;
    if (offset < ram.size()) return ram[offset];
    return 0xFF;
}

void MBC1::writeRAM(uint16_t address, uint8_t value) {
    if (!ramEnabled) return;

    uint16_t offset = address - 0xA000;
    if (offset < ram.size()) ram[offset] = value;
}