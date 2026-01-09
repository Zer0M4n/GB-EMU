#ifndef CARTRIDGE_H
#define CARTRIDGE_H

class cartridge
{
private:
    /* data */
    std::vector<uint8_t> ROM;
public:
    std::vector<uint8_t> loadRom(const std::string& path);
    ~cartridge();
};

#endif 