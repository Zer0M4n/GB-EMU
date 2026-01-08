#ifndef MMU_H
#define MMU_H

#include <array>
#include <cstdint>

class mmu
{
private:
    // ===== Memoria =====
    std::array<uint8_t, 0x8000> ROM;   // 32KB ROM
    std::array<uint8_t, 0x2000> VRAM;  // 8KB Video RAM
    std::array<uint8_t, 0x2000> WRAM;  // 8KB Work RAM
    std::array<uint8_t, 0x007F> HRAM;  // 127 bytes High RAM
    std::array<uint8_t, 0x0080> IO;    // 128 bytes I/O
    std::array<uint8_t, 0x00A0> OAM;   // Sprite Attribute Table

    uint8_t IE; // Interrupt Enable

    // ===== Helpers =====
    uint16_t offSet(uint16_t direction, uint16_t base);
    void DMA(uint8_t value);

public:
    uint8_t readMemory(uint16_t direction);
    void writeMemory(uint16_t direction, uint8_t value);
};

#endif // MMU_H
