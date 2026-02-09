// ============================================================
// MMU.H - ACTUALIZADO CON SOPORTE PARA JOYPAD
// ============================================================

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "cartridge/cartridge.h"

class ppu; // Forward declaration
class timer; // Forward declaration
class cpu; // Forward declaration
class emcc_main; // Forward declaration
class APU; // Forward declaration - Audio Processing Unit

class mmu
{
    friend class ppu;
    friend class timer;
    friend class cpu;
    friend class emcc_main;
    
public:
    // Constructor explícito que recibe la ruta
    explicit mmu(const std::string& romPath);

    uint8_t readMemory(uint16_t address);
    void writeMemory(uint16_t address, uint8_t value);
    
    // ============================================================
    // NUEVA: Función para establecer estado de botones
    // ============================================================
    void setButton(int button_id, bool pressed);
    
    // ============================================================
    // NUEVA: Función para conectar la APU
    // ============================================================
    void setAPU(APU* apu_ptr);

private:
    // Instancia del cartucho
    cartridge cart;
    
    // Puntero a la APU (Audio)
    APU* apu = nullptr;

    // Regiones de memoria interna
    std::array<uint8_t, 0x2000> VRAM; // 8KB Video RAM
    std::array<uint8_t, 0x2000> WRAM; // 8KB Work RAM
    std::array<uint8_t, 0x007F> HRAM; // 127 bytes High RAM
    std::array<uint8_t, 0x0080> IO;   // 128 bytes I/O
    std::array<uint8_t, 0x00A0> OAM;  // Object Attribute Memory

    uint8_t IE; // Interrupt Enable (0xFFFF)
    uint8_t IF; // Interrupt Flag (0xFF0F)
    
    // ============================================================
    // NUEVAS: Variables para el estado de los botones
    // ============================================================
    bool button_right = false;
    bool button_left = false;
    bool button_up = false;
    bool button_down = false;
    bool button_a = false;
    bool button_b = false;
    bool button_select = false;
    bool button_start = false;
    
    // Funciones auxiliares privadas
    uint16_t offSet(uint16_t address, uint16_t base);
    void DMA(uint8_t value);
};

// ============================================================
// NOTAS:
// ============================================================
// 1. Añadida función setButton() para actualizar estado de botones
// 2. Añadidas variables privadas para almacenar estado de cada botón
// 3. Estas variables se usan en readMemory() cuando se lee 0xFF00