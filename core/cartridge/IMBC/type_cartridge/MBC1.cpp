#include "IMBC.h"
#include <iostream>
#include <vector>

class MBC1 : public IMBC
{
    private:
        std::vector<uint8_t>& romRef;
        uint8_t rom_bank = 1;
        uint8_t ram_bank = 1;
        bool ram_enabled = false;
        uint8_t banking_mode = 0;
        
    public:
        MBC1(std::vector<uint8_t>& rom) : romRef(rom) {};
        uint8_t readROM(uint16_t address) override
        {

        }
        ~MBC1();
};
