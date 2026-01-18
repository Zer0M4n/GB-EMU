#include <iostream>
#include <array>
#include <vector>
#include <fstream>
#include <cartridge.h>
class mmu
{ 
    cartridge cart;
    private:
            std::array<uint8_t, 0x2000>  VRAM; // 8KB VIDEO RAM
            std::array<uint8_t, 0x2000>  WRAM; // 8KB WORK RAM
            std::array<uint8_t, 0x007F>   HRAM; // 127 Bytes (High RAM)
            std::array<uint8_t, 0x0080> IO; // 128 Bytes input/outpur
            std::array<uint8_t, 0x00A0> OAM; // Object Attribute memory

            uint8_t IE; //INTERRUPT ENABLE

            uint16_t offSet(uint16_t direction, uint16_t var)
            {
                return direction - var;
            }
            void DMA(uint8_t value) //Direct memory access 
            {
                uint8_t DataCopy;
                for (int i = 0; i < OAM.size() ; i++)
                {
                    DataCopy = readMemory((value << 8) + i);
                    OAM[i] = DataCopy;
                }
                
            }        
    public:
        uint8_t readMemory(uint16_t direction) 
        {   
            if (direction == 0xFFFF)
            {
                return IE;
            }
            if(0x0000 <= direction && direction <= 0x7FFF) //ROM cartridge
            {   
                return cart.readCartridge(direction);
            }
            else if (0x8000 <= direction && direction <= 0x9FFF ) // VRAM
            {
                return VRAM[offSet(direction , 0x8000)];
            }
            else if (0xA000 <= direction && direction <= 0XBFFF)
            {
                return cart.readCartridge(direction);
            }
            
            else if (0xC000 <= direction && direction <=  0xDFFF) // WRAM
            {
                return  WRAM[offSet(direction , 0xC000)];
            }
            else if (0xE000 <= direction && direction <= 0xFDFF) // ECHO RAM
            {
                return  WRAM[offSet(direction , 0xE000)];
            }
            
             else if (0xFE00 <= direction && direction <= 0xFE9F ) //OAM
            {
                return OAM[offSet(direction , 0xFE00)];
            }
            else if (0xFF00 <= direction && direction <= 0xFF46) //IO
            {
                return IO[offSet(direction , 0xFF00)];
                
            }
            else if (0xFF80 <= direction && direction <= 0xFFFE) // HRAM
            {
                return HRAM[offSet(direction, 0xFF80)];
            }
            else
            {
                std::cout << "Memory address error, not found " << "\n;";
                return NULL;
            }

            
        }

        void writeMemory(uint16_t direction , uint8_t value) 
        {
            if (direction == 0xFF46)
            {
                DMA(value);
                return;
            }
            if (direction == 0xFFFF)
            {
                IE = value;
                return;
            }

            if (direction <= 0x7FFF)
            {
                cart.writeCartridge(direction, value);
            }
            else if (0x8000 <= direction && direction <= 0x9FFF ) // VRAM
            {
                VRAM[offSet(direction , 0x8000)] = value;
            }
            else if (0xA000 <= direction && direction <= 0XBFFF)
            {
                cart.writeCartridge (direction, value);
            }
            else if (0xC000 <= direction && direction <=  0xDFFF) // WRAM
            {
                WRAM[offSet(direction , 0xC000)] = value;
            }
            else if (0xFE00 <= direction && direction <= 0xFE9F ) //OAM
            {
                OAM[offSet(direction , 0xFE00)] = value;
            }
            else if (0xFF00 <= direction && direction <= 0xFF46) //IO
            {
                IO[offSet(direction , 0xFF00)] = value;
                
            }
            else if (0xFF80 <= direction && direction <= 0xFFFE) // HRAM
            {
                HRAM[offSet(direction, 0xFF80)] = value;
            }
            else
            {
                std::cout << "Memory address error, not found " << "\n;";
            }
        }

    };
