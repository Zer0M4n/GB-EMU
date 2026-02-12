#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "IMBC/IMBC.h"

class cartridge
{
public:
    explicit cartridge(const std::string& path);
    ~cartridge();

    uint8_t  readCartridge(uint16_t address);
    void     writeCartridge(uint16_t address, uint8_t value);

    const std::string& getTitle()         const { return Title; }
    uint8_t            getCartridgeType() const { return cartridge_type; }
    bool               isLoaded()         const { return !ROM.empty() && mbc != nullptr; }

private:
    // ROM y RAM crudas
    std::vector<uint8_t> ROM;
    std::vector<uint8_t> RAM;

    // MBC polimórfico
    std::unique_ptr<IMBC> mbc;

    // Metadata del header
    std::string Title;
    uint8_t     cartridge_type  = 0;
    uint8_t     ROM_type        = 0;
    uint8_t     RAM_type        = 0;
    uint16_t    rom_banks_count = 2;

    // Helpers privados
    bool loadRom(const std::string& path);
    void parseHeader();
    bool verifyChecksum();
    void Get_Title();
    void setCartridgeType();
    uint16_t resolveRomBanks(uint8_t rom_byte);
    void     resolveRamSize(uint8_t ram_byte);
    void     createMBC();
};