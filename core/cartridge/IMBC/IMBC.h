#ifndef IMBC_H
#define IMBC_H

#include <fstream>
#include <iostream>

class IMBC
{
    public:
        virtual ~IMBC() = default;

        //
        virtual uint8_t readROM(uint16_t address) = 0;
        virtual void writeROM(uint16_t address, uint8_t value) = 0;
        virtual uint8_t readRAM(uint16_t address) = 0;
        virtual void writeRAM(uint16_t address, uint8_t value) = 0;
};
#endif // CARTRIDGE_H