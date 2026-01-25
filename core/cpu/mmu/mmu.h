#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Ajusta este include si tu carpeta cartridge está en otro nivel, 
// pero según tu CMake, esto debería funcionar:
#include "cartridge/cartridge.h"

class mmu
{
public:
    // Constructor explícito que recibe la ruta
    explicit mmu(const std::string& romPath);

    uint8_t readMemory(uint16_t address);
    void writeMemory(uint16_t address, uint8_t value);

private:
    // Instancia del cartucho
    cartridge cart;

    // Regiones de memoria interna
    std::array<uint8_t, 0x2000> VRAM; // 8KB Video RAM
    std::array<uint8_t, 0x2000> WRAM; // 8KB Work RAM
    std::array<uint8_t, 0x007F> HRAM; // 127 bytes High RAM
    std::array<uint8_t, 0x0080> IO;   // 128 bytes I/O
    std::array<uint8_t, 0x00A0> OAM;  // Object Attribute Memory

    uint8_t IE; // Interrupt Enable (0xFFFF)
    uint8_t IF; // Interrupt flag (0xFFFF)
    
    // Funciones auxiliares privadas
    uint16_t offSet(uint16_t address, uint16_t base);
    void DMA(uint8_t value);
};