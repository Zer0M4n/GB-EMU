#include <vector>
#include <iostream>
#include "cartridge/IMBC/IMBC.h"
class RomOnly : public IMBC {
    private:
        std::vector<uint8_t>& romRef;
        // Eliminamos la referencia a RAM si el cartucho no la tiene físicamente

    public:
        RomOnly(std::vector<uint8_t>& rom) : romRef(rom) {}

        uint8_t readROM(uint16_t address) override {
            // Validación básica de rango para evitar salidas del vector
            if (address < romRef.size()) {
                return romRef[address];
            }
            return 0xFF;
        }

        void writeROM(uint16_t address, uint8_t value) override {
            // No sucede nada: es solo lectura y no hay registros de control
        }

        uint8_t readRAM(uint16_t address) override {
            return 0xFF; // No hay RAM física conectada
        }

        void writeRAM(uint16_t address, uint8_t value) override {
            // No hay donde escribir
        }
};
