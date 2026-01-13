#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

// Forward declarations
class IMBC;

class cartridge
{
private:
    std::vector<uint8_t> ROM;
    uint8_t cartridge_type;
    std::unique_ptr<IMBC> mbc;
    std::string Title;
    std::vector<uint8_t> Entry_Point;

    void parseHeader();
    void Get_Title();
    void cartridgeType();

public:
    explicit cartridge(const std::string& path);
    ~cartridge();

    bool loadRom(const std::string& path);

    uint8_t readCartridge(uint16_t address);
    void writeCartridge(uint16_t address, uint8_t value);
};

#endif // CARTRIDGE_H
