 #include <iostream>
 #include <array>
class mmu
{
    
    private:
        private:
            std::array<uint8_t, 32768> ROM;  // 32KB (Bancos 0 y 1)
            std::array<uint8_t, 8192>  VRAM; // 8KB VIDEO RAM
            std::array<uint8_t, 8192>  WRAM; // 8KB WORK RAM
            std::array<uint8_t, 127>   HRAM; // 127 Bytes (High RAM)

            uint16_t offSet()
            {
                
            }
        
        
    public:

        void readMemory(uint16_t direction) 
        {

        }

        void writeMemory(uint16_t direction)
        {

        }
};
