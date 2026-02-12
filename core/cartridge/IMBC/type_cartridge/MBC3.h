#pragma once
#include "../IMBC.h"
#include <cstdint>
#include <ctime>
#include <vector>

class MBC3 : public IMBC
{
public:
    MBC3(std::vector<uint8_t>& rom_ref,
         std::vector<uint8_t>& ram_ref,
         uint16_t              banks);

    uint8_t readROM(uint16_t address) override;
    void    writeROM(uint16_t address, uint8_t value) override;
    uint8_t readRAM(uint16_t address) override;
    void    writeRAM(uint16_t address, uint8_t value) override;

private:
    std::vector<uint8_t>& rom;
    std::vector<uint8_t>& ram;

    uint16_t totalRomBanks;
    uint8_t  romBank;         // Banco activo (1-127)
    uint8_t  ramRtcSelect;    // 0x00-0x03 = RAM bank, 0x08-0x0C = RTC
    bool     ramEnabled;

    // RTC
    uint8_t  rtcRegisters[5];        // Valores en tiempo real
    uint8_t  latchedRtcRegisters[5]; // Snapshot congelado por el juego
    uint8_t  latchValue;             // Último valor escrito en 0x6000-0x7FFF

    void updateRTC();
};