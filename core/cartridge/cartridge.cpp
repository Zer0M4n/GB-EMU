#include "cartridge.h"
#include "IMBC/type_cartridge/RomOnly.h"
#include "IMBC/type_cartridge/MBC1.h"
#include "IMBC/type_cartridge/MBC3.h"
#include <fstream>
#include <iostream>

// ============================================================
//  Constructor / Destructor
// ============================================================

cartridge::cartridge(const std::string& path)
{
    if (loadRom(path))
        parseHeader();
}

cartridge::~cartridge() = default;

// ============================================================
//  Carga de ROM
// ============================================================

bool cartridge::loadRom(const std::string& path)
{
    std::ifstream rom(path, std::ios::binary | std::ios::ate);
    if (!rom.is_open())
    {
        std::cerr << "[Cartridge] ERROR: ROM no encontrada en: " << path << "\n";
        return false;
    }

    std::streamsize size = rom.tellg();
    if (size <= 0)
    {
        std::cerr << "[Cartridge] ERROR: Archivo vacío o inválido.\n";
        return false;
    }

    ROM.resize(static_cast<size_t>(size));
    rom.seekg(0, std::ios::beg);

    if (!rom.read(reinterpret_cast<char*>(ROM.data()), size))
    {
        std::cerr << "[Cartridge] ERROR: Fallo al leer el archivo.\n";
        ROM.clear();
        return false;
    }

    std::cout << "[Cartridge] ROM cargada. Tamaño: " << size << " bytes.\n";
    return true;
}

// ============================================================
//  Lectura / Escritura pública
// ============================================================

uint8_t cartridge::readCartridge(uint16_t address)
{
    if (!mbc) return 0xFF;

    if (address <= 0x7FFF)
        return mbc->readROM(address);

    if (address >= 0xA000 && address <= 0xBFFF)
        return mbc->readRAM(address);

    return 0xFF;
}

void cartridge::writeCartridge(uint16_t address, uint8_t value)
{
    if (!mbc) return;

    if (address <= 0x7FFF)
        mbc->writeROM(address, value);
    else if (address >= 0xA000 && address <= 0xBFFF)
        mbc->writeRAM(address, value);
}

// ============================================================
//  Helpers privados
// ============================================================

bool cartridge::verifyChecksum()
{
    if (ROM.size() < 0x014E)
    {
        std::cerr << "[Cartridge] ROM demasiado pequeña para checksum.\n";
        return false;
    }

    uint8_t x = 0;
    for (uint16_t i = 0x0134; i <= 0x014C; ++i)
        x = x - ROM[i] - 1;

    bool ok = (x == ROM[0x014D]);
    if (!ok)
        std::cerr << "[Cartridge] Checksum FALLO. Calc: "
                  << (int)x << " Leído: " << (int)ROM[0x014D] << "\n";
    return ok;
}

void cartridge::Get_Title()
{
    Title.clear();
    // 0x0134-0x0143: título en ASCII (bytes nulos terminan el string)
    for (uint16_t i = 0x0134; i <= 0x0143; ++i)
    {
        if (ROM[i] == 0) break;
        Title += static_cast<char>(ROM[i]);
    }
    std::cout << "[Cartridge] Título: " << Title << "\n";
}

void cartridge::setCartridgeType()
{
    cartridge_type = ROM[0x0147];
}

uint16_t cartridge::resolveRomBanks(uint8_t rom_byte)
{
    // Cada valor es: (32KB << N) / 16KB = 2 << N bancos
    switch (rom_byte)
    {
        case 0x00: return 2;    //  32 KB
        case 0x01: return 4;    //  64 KB
        case 0x02: return 8;    // 128 KB
        case 0x03: return 16;   // 256 KB
        case 0x04: return 32;   // 512 KB
        case 0x05: return 64;   //   1 MB
        case 0x06: return 128;  //   2 MB  ← Pokémon Rojo/Azul
        case 0x07: return 256;  //   4 MB
        case 0x08: return 512;  //   8 MB
        default:
            std::cerr << "[Cartridge] ROM_type desconocido: 0x"
                      << std::hex << (int)rom_byte << std::dec << "\n";
            return 2;
    }
}

void cartridge::resolveRamSize(uint8_t ram_byte)
{
    size_t size = 0;
    switch (ram_byte)
    {
        case 0x00: size = 0;          break; // Sin RAM
        case 0x01: size = 0x800;      break; // 2 KB  (raro)
        case 0x02: size = 0x2000;     break; // 8 KB
        case 0x03: size = 0x8000;     break; // 32 KB  ← Pokémon Rojo
        case 0x04: size = 0x20000;    break; // 128 KB
        case 0x05: size = 0x10000;    break; // 64 KB
        default:
            std::cerr << "[Cartridge] RAM_type desconocido: 0x"
                      << std::hex << (int)ram_byte << std::dec << "\n";
            break;
    }

    if (size > 0)
    {
        RAM.assign(size, 0xFF);  // ← IMPORTANTE: inicializar a 0xFF, no a 0x00
        std::cout << "[Cartridge] RAM: " << size << " bytes inicializada.\n";
    }
}

void cartridge::createMBC()
{
    std::cout << "[Cartridge] Tipo MBC: 0x" << std::hex << (int)cartridge_type
              << std::dec << " | ROM banks: " << rom_banks_count << "\n";

    switch (cartridge_type)
    {
        // ---- ROM Only ----
        case 0x00:
            mbc = std::make_unique<RomOnly>(ROM);
            std::cout << "[Cartridge] MBC: RomOnly\n";
            break;

        // ---- MBC1 ----
        case 0x01:  // MBC1
        case 0x02:  // MBC1 + RAM
        case 0x03:  // MBC1 + RAM + BATTERY
            mbc = std::make_unique<MBC1>(ROM, RAM, rom_banks_count);
            std::cout << "[Cartridge] MBC: MBC1\n";
            break;

        // ---- MBC3 ----
        case 0x0F:  // MBC3 + TIMER + BATTERY
        case 0x10:  // MBC3 + TIMER + RAM + BATTERY
        case 0x11:  // MBC3
        case 0x12:  // MBC3 + RAM
        case 0x13:  // MBC3 + RAM + BATTERY  ← Pokémon Rojo
            mbc = std::make_unique<MBC3>(ROM, RAM, rom_banks_count);
            std::cout << "[Cartridge] MBC: MBC3\n";
            break;

        default:
            std::cerr << "[Cartridge] Tipo MBC no implementado: 0x"
                      << std::hex << (int)cartridge_type << std::dec
                      << " → usando RomOnly como fallback.\n";
            mbc = std::make_unique<RomOnly>(ROM);
            break;
    }
}

void cartridge::parseHeader()
{
    if (ROM.size() < 0x0150)
    {
        std::cerr << "[Cartridge] ROM demasiado pequeña para tener header válido.\n";
        return;
    }

    verifyChecksum();  // Advertencia, no aborta
    Get_Title();
    setCartridgeType();

    ROM_type        = ROM[0x0148];
    rom_banks_count = resolveRomBanks(ROM_type);

    RAM_type        = ROM[0x0149];
    resolveRamSize(RAM_type);   // ← RAM se dimensiona ANTES de crear el MBC

    createMBC();                // ← MBC recibe referencias ya válidas
}