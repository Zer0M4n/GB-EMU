#ifndef PPU_H
#define PPU_H
#pragma once
#include <vector>
#include <cstdint>
#include "mmu/mmu.h"

class ppu {
public:
    ppu(mmu& mmu_ref);
    void step(int cycles);

    // Buffer de píxeles: 160 de ancho x 144 de alto
    // Usamos uint32_t para formato RGBA o ARGB directo (0xFFRRGGBB)
    std::vector<uint32_t> gfx; 
    
    // Bandera para saber si el frame terminó y se puede pintar en pantalla
    bool frame_complete;

private:
    mmu& memory;
    int scanline_counter;
    uint32_t palette[4];

    void update_stat_register();
    void draw_scanline();
};

#endif 