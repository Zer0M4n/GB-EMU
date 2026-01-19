#pragma once
#include "../IMBC.h"
#include <vector>

class RomOnly : public IMBC {
private:
    std::vector<uint8_t>& rom; // Referencia a la ROM cargada en Cartridge

public:
    explicit RomOnly(std::vector<uint8_t>& rom_ref);
    
    uint8_t readROM(uint16_t address) override;
    void writeROM(uint16_t address, uint8_t value) override;
    
    uint8_t readRAM(uint16_t address) override;
    void writeRAM(uint16_t address, uint8_t value) override;
};