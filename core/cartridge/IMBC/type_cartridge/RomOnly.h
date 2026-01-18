#ifndef ROM_ONLY_H
#define ROM_ONLY_H

#include <vector>
#include <cstdint>
#include "IMBC.h"

class RomOnly : public IMBC
{
private:
    std::vector<uint8_t>& romRef;

public:
    explicit RomOnly(std::vector<uint8_t>& rom);

    uint8_t readROM(uint16_t address) override;
    void writeROM(uint16_t address, uint8_t value) override;

    uint8_t readRAM(uint16_t address) override;
    void writeRAM(uint16_t address, uint8_t value) override;
};

#endif // ROM_ONLY_H
