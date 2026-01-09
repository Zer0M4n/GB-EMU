 #include <iostream>
 #include <array>
 #include "mmu.h"
 #include "intructions.h"
class cpu
{
    private:
        enum R8
        {
            A = 0,
            F = 1,
            B = 2,
            C = 3,
            D = 4,
            E = 5,
            H = 6,
            L = 7,
        };
        
        std::array<uint8_t, 8> r8; // register 
        uint16_t SP; //stack pointer
        uint16_t PC; //program countet

        using Intructions = void(cpu::*)(uint8_t);

        std::array<Intructions,1> table_opcode {};

        //FLAGS (register F is combinacion this)
        
        //zero flag
        //substraction flag
        //half carry flag
        //carry flag

        uint8_t getZ() const { return (r8[F] >> 7) & 1 ; } 
        uint8_t getN() const { return (r8[F] >> 6) & 1 ; }
        uint8_t getH() const { return (r8[F] >> 5) & 1 ; }
        uint8_t getC() const { return (r8[F] >> 4) & 1 ; }

        void setZero() { r8[F] |= (1 << 7); }
        void setSubstraction() { r8[F] |= (1 << 6); }
        void setHalf() { r8[F] |= (1 << 5); }
        void setCarry() { r8[F] |= (1 << 4); }
        
        //GETERS AND SETERS VIRTUAL REGISTERS 16 BITS

        uint16_t getAF() const 
        { return 
            (r8[A] << 8) | 
            (r8[F] & 0xF0); 
        }
        void setAF(uint16_t val) 
        { 
            r8[A] = val >> 8; 
            r8[F] = val & 0xF0; 
        }

        uint16_t getBC() const {return (r8[B] << 8) | r8[C];}
        void setBC(const uint16_t val) 
        {
            r8[B]  = val>> 8;
            r8[C] = val & 0xFF;
        }

        uint16_t getDE() const {return (r8[D] << 8) | r8[E]; }
        void setDE(const uint16_t val) 
        {
            r8[D]  = val >> 8;
            r8[E] = val & 0xFF;
        }

        uint16_t getHL() const {return (r8[H] << 8) | r8[L];}
        void setHL(const uint16_t val) 
        {
            r8[H]  = val >> 8;
            r8[L] = val & 0xFF;
        }

        //INTRUCTIONS
        void NOP(uint8_t var)
        {

        }
        
    public:
        cpu()
        {
            table_opcode.fill(nullptr);
            table_opcode[0x0] = &cpu::NOP;
        }
        uint8_t fetch(mmu& memory)
        {
            uint8_t opcode = memory.readMemory(PC);
            this->PC++;
            return opcode;
        }

        void step(mmu& memory)
        {
            uint8_t opcode = fetch(memory);

            //DECODE and EXCUTE
            if (table_opcode[opcode])
            {
                (this->*table_opcode[opcode])(opcode);
            } 
            else
            {
                std::cout << "opcode not impleted or not exist" << "\n";
            }
        }
};


