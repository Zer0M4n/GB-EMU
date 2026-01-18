#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <memory> // include
#include "IMBC/IMBC.h"
#include "RomOnly.h"

class cartridge
{
    private:
        std::vector<uint8_t> ROM ;
        uint8_t cartridge_type;
        std::unique_ptr<IMBC> mbc;
        std::string Title;
        std::vector<uint8_t> Entry_Point;
        void Get_Title()
        {
            for (uint16_t i = 0x0134; i <= 0x0143; i++)
            {
                if (ROM[i] == 0)
                    break;
                Title += static_cast<char>(ROM[i]);

                std::cout << "Titulo " << Title << "\n";
                
            }
        }
        void cartridgeType()
        {
            cartridge_type = ROM[0x147];
        }

        void parseHeader() //fuctions load headers in vectors
        {
            Get_Title();
            cartridgeType();
            switch (cartridge_type)
            {
                case 0x00:
                mbc = std::make_unique<RomOnly>(ROM);
                    break;
                
                default:
                    break;
            }
        }

    public:
        cartridge(const std::string &path)
        {
            if(loadRom(path))
                parseHeader();
            
        }
        
        ~cartridge();

        bool loadRom(const std::string &path)
        {
            std::ifstream rom(path, std::ios::binary | std::ios::ate);

            if (rom.is_open())
            {
                std::streamsize size = rom.tellg();
                ROM.resize(size);

                rom.seekg(0, std::ios::beg);

                if (rom.read(reinterpret_cast<char *>(ROM.data()), size))
                {
                    return true;
                    std::cout << "ROM LOAD , SIZE : " << size << "\n";
                }
            }
            else
            {
                return false;
                printf("ERROR: ROM not found");
            }
        }


        uint8_t readCartridge(uint16_t address) 
        {
        if (!mbc) return 0xFF; // Seguridad por si no hay MBC cargado

                if (address >= 0x0000 && address <= 0x7FFF) {
                    return mbc->readROM(address);
                } else if (address >= 0xA000 && address <= 0xBFFF) {
                    return mbc->readRAM(address);
                }
                return 0xFF;
        }
        void writeCartridge(uint16_t address, uint8_t value)
        {
            if(!mbc) return;

            if (address >= 0x0000 && address <= 0x7FFF)
            {
                mbc->writeROM(address, value);
            } else if (address >= 0xA000 && address <= 0xBFFF)
            {
                mbc->writeRAM(address,value);
            }
            
            
        }

};
