#include "cartridge/IMBC/IMBC.h"
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
            if(address <= 0x3FFF)
            {
                return romRef[address];
            }
            else if(0x4000 <= address && address <= 0x7FFF )
            {
                return romRef[(address - 0x4000) + (0x4000 * rom_bank)];
            }
            else
            {
                return NULL;
            }
        }
        void writeROM(uint16_t address, uint8_t value) override {
            if(address <= 0x1FFF)
            {
                ram_enabled = true;
            }
            else if(0x2000 <= address && address <= 0x3FFF)
            {
                rom_bank = value & 0x1F;
                if (rom_bank == 0)
                {
                    rom_bank = 1;
                }
                
            }
            else if(0x4000 <= address && address <= 0x5FFF)
            {

            }
            else if (0x6000 <= address && address <= 0x7FFF)
            {

            }
            else
            {
                return;
            }
        }
        
        ~MBC1();
};
