#include "RomOnly.h"

// Constructor: Guardamos la referencia a la ROM
RomOnly::RomOnly(std::vector<uint8_t>& rom_ref) : rom(rom_ref) {}

uint8_t RomOnly::readROM(uint16_t address) {
    if (address < rom.size())
        return rom[address];
    return 0xFF;
}

void RomOnly::writeROM(uint16_t address, uint8_t value) {
    // RomOnly no permite escribir en ROM (no tiene bancos)
    (void)address; (void)value; 
}

uint8_t RomOnly::readRAM(uint16_t address) {
    (void)address;
    return 0xFF; // No tiene RAM
}

void RomOnly::writeRAM(uint16_t address, uint8_t value) {
    (void)address; (void)value; // No tiene RAM
}