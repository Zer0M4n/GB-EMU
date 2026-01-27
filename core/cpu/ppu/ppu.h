#ifndef PPU_H
#define PPU_H
#pragma once

#include <array>
#include <cstdint>
#include "mmu/mmu.h"

class ppu
{
    private:
        mmu& memory;
        // --- Control de Tiempos ---
        // La PPU tiene 4 modos (HBlank, VBlank, OAM, Transfer)
        // scanline_counter cuenta los ciclos dentro de una línea
        int scanline_counter; 

        // --- Paleta ---
        // Guardamos los 4 colores "Matrix" aquí para acceso rápido
        std::array<uint32_t, 4> palette;

        // --- Helpers Internos ---
        // Funciones que implementaremos luego para dibujar
        void draw_scanline();
        void update_stat_register();
    public:
        ppu(mmu& mmu_ref);
        void step(int cycles);
        std::array<uint32_t, 160 *144> gfx;
    
};

#endif 