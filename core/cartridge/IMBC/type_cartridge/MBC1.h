#pragma once
#include "../IMBC.h"
#include <vector>

class MBC1 : public IMBC {
private:
    std::vector<uint8_t>& rom;
    std::vector<uint8_t>& ram;
    
    uint8_t romBank;
    uint8_t ramBank;
    bool ramEnabled;
    uint8_t bankingMode; 
    uint16_t romBanksCount;

public:
    // Nota: El constructor debe coincidir con la llamada en cartridge.cpp
    MBC1(std::vector<uint8_t>& rom_ref, std::vector<uint8_t>& ram_ref, uint16_t banks);

    uint8_t readROM(uint16_t address) override;
    void writeROM(uint16_t address, uint8_t value) override;
    
    uint8_t readRAM(uint16_t address) override;
    void writeRAM(uint16_t address, uint8_t value) override;
};