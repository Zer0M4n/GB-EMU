 #include <iostream>
 #include <array>
class mmu
{
    
    private:
        private:
            std::array<uint8_t, 0x8000> ROM;  // 32KB (Bancos 0 y 1)
            std::array<uint8_t, 0x2000>  VRAM; // 8KB VIDEO RAM
            std::array<uint8_t, 0x2000>  WRAM; // 8KB WORK RAM
            std::array<uint8_t, 0x007F>   HRAM; // 127 Bytes (High RAM)

            uint16_t offSet(uint16_t direction, uint16_t var)
            {
                return direction - var;
            }
        
        
    public:

        uint8_t readMemory(uint16_t direction) 
        {   
            
            if(0x0000 <= direction && direction <= 0x7FFF) //ROM cartridge
            {   
                return ROM[direction];
            }
            else if (0x8000 <= direction && direction <= 0x9FFF ) // VRAM
            {
                return VRAM[offSet(direction , 0x8000)];
            }
            else if (0xC000 <= direction && direction <=  0xDFFF) // WRAM
            {
                return  WRAM[offSet(direction , 0xC000)];
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

        void writeMemory(uint16_t direction , uint8_t value) // only write memory for VRAM ,WRAM and HRAM
        {
            if (0x8000 <= direction && direction <= 0x9FFF ) // VRAM
            {
                VRAM[offSet(direction , 0x8000)] = value;
            }
            else if (0xC000 <= direction && direction <=  0xDFFF) // WRAM
            {
                WRAM[offSet(direction , 0xC000)] = value;
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
