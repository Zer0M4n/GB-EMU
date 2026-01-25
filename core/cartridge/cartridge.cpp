#include "cartridge.h"
#include <fstream>
#include <iostream>

// INCLUDES DE TUS MBC (Asegúrate que estas rutas existan en tu proyecto)
#include "IMBC/IMBC.h" 
#include "IMBC/type_cartridge/RomOnly.h"
#include "IMBC/type_cartridge/MBC1.h"

// --- Constructor ---
cartridge::cartridge(const std::string &path)
{
    if(loadRom(path))
    {
        parseHeader();
    }
}

// --- Destructor ---
cartridge::~cartridge() = default; 

// --- Carga de ROM ---
bool cartridge::loadRom(const std::string &path)
{
    std::ifstream rom(path, std::ios::binary | std::ios::ate);

    if (!rom.is_open())
    {
        std::cerr << "ERROR: ROM not found at " << path << "\n";
        return false;
    }

    std::streamsize size = rom.tellg();
    ROM.resize(size);

    rom.seekg(0, std::ios::beg);

    if (rom.read(reinterpret_cast<char *>(ROM.data()), size))
    {
        std::cout << "ROM LOADED. SIZE: " << size << " bytes.\n";
        return true;
    }
    
    return false;
}

// --- Lectura de Memoria ---
uint8_t cartridge::readCartridge(uint16_t address) 
{
    if (!mbc) return 0xFF; // Seguridad

    if (address <= 0x7FFF) {
        return mbc->readROM(address);
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        return mbc->readRAM(address);
    }
    return 0xFF;
}

// --- Escritura de Memoria ---
void cartridge::writeCartridge(uint16_t address, uint8_t value)
{
    if(!mbc) return;

    if (address <= 0x7FFF) {
        mbc->writeROM(address, value);
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        mbc->writeRAM(address, value);
    }
}

// --- Helpers Privados ---

bool cartridge::verifyChecksum() 
{
    uint8_t x = 0;
    // Rango oficial Checksum: 0x0134 a 0x014C
    for (uint16_t i = 0x0134; i <= 0x014C; i++) {
        x = x - ROM[i] - 1;
    }

    uint8_t actual_checksum = ROM[0x014D];

    if (x == actual_checksum) {
        // std::cout << "Checksum CORRECTO\n";
        return true;
    } else {
        std::cerr << "Checksum FAIL. Calc: " << (int)x << " Read: " << (int)actual_checksum << "\n";
        return false;
    }
}

void cartridge::Get_Title()
{
    Title.clear();
    for (uint16_t i = 0x0134; i <= 0x0143; i++)
    {
        if (ROM[i] == 0) break;
        Title += static_cast<char>(ROM[i]);
    }
    std::cout << "Game Title: " << Title << "\n";
}

void cartridge::setCartridgeType()
{
    cartridge_type = ROM[0x147];
}

void cartridge::parseHeader()
{
    if(!verifyChecksum()) {
        std::cerr << "Warning: Checksum failed!\n";
    }

    Get_Title();
    setCartridgeType();

    // 1. ROM SIZE
    ROM_type = ROM[0x148];
    switch (ROM_type)
    {
        case 0x00: rom_banks_count = 2; break; // 32 KB
        case 0x01: rom_banks_count = 4; break; // 64 KB
        case 0x02: rom_banks_count = 8; break; // 128 KB
        // Añadir más casos según sea necesario
        default: rom_banks_count = 2; break;
    }

    // 2. RAM SIZE
    RAM_type = ROM[0x149];
    switch (RAM_type)
    {
        case 0x02: RAM.resize(0x2000); break;   // 8KB
        case 0x03: RAM.resize(0x8000); break;   // 32KB
        case 0x04: RAM.resize(0x20000); break;  // 128KB
        case 0x05: RAM.resize(0x10000); break;  // 64KB
        default: break; // Sin RAM
    }

    // 3. Crear el MBC correcto
    std::cout << "Cartridge Type: " << std::hex << (int)cartridge_type << "\n";
    
    switch (cartridge_type) 
    {
        case 0x00: // ROM Only
            mbc = std::make_unique<RomOnly>(ROM);
            std::cout << "IMBC : RomONly" << "\n";
            break;
        case 0x01: // MBC1
        case 0x02: // MBC1 + RAM
        case 0x03: // MBC1 + RAM + BATTERY
            mbc = std::make_unique<MBC1>(ROM, RAM, rom_banks_count);
            std::cout << "IMBC : RomONly" << "\n";
            break;
        
        default:
            std::cerr << "MBC Type not implemented: " << std::hex << (int)cartridge_type << "\n";
            // Fallback a RomOnly para evitar crash inmediato
            mbc = std::make_unique<RomOnly>(ROM);
            break;
    } 
}