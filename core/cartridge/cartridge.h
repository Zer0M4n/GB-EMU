#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// Forward declaration (Declaración adelantada para evitar dependencias circulares)
class IMBC;

class cartridge
{
private:
    std::vector<uint8_t> ROM;
    std::vector<uint8_t> RAM;
    
    // Metadata
    std::string Title;
    uint8_t cartridge_type;
    uint8_t ROM_type;
    uint8_t RAM_type;
    uint16_t rom_banks_count;
    
    // Puntero al Memory Bank Controller
    std::unique_ptr<IMBC> mbc;

    // Funciones internas (Helpers)
    void parseHeader();
    void Get_Title();
    void setCartridgeType(); // Renombrado para claridad
    bool verifyChecksum();

public:
    explicit cartridge(const std::string& path);
    ~cartridge(); // El destructor debe estar definido en el .cpp por el unique_ptr

    bool loadRom(const std::string& path);

    uint8_t readCartridge(uint16_t address);
    void writeCartridge(uint16_t address, uint8_t value);
};

#endif // CARTRIDGE_H