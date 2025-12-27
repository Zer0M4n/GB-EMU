 #include <iostream>
 #include <array>
class cpu
{
private:
     
public:
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

    //FLAGS (register F is combinacion this)
    uint8_t z; //zero flag
    uint8_t n; //substraction flag
    uint8_t h; //half carry flag
    uint8_t c; //carry flag


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
    
};


