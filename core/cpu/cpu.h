#pragma once

#include <array>
#include <cstdint>
#include <iostream>

// Asegúrate de que la ruta a mmu.h sea correcta según tu proyecto
#include "mmu/mmu.h" 

class cpu
{
public:
    // Constructor
    cpu(mmu& mmu_ref);

    // Función principal para avanzar un paso
    int step();

private:
    // Referencia a la memoria (MMU)
    mmu& memory;

    // Registros
    uint16_t PC; // Program Counter
    uint16_t SP; // Stack Pointer
    std::array<uint8_t, 8> r8; // Registros de 8 bits

    // Enum para acceder al array r8 más fácil
    enum R8 {
        A = 0, F = 1, 
        B = 2, C = 3, 
        D = 4, E = 5, 
        H = 6, L = 7
    };

    // --- FLAGS (Registro F) ---
    // Z=Zero, N=Subtraction, H=Half Carry, C=Carry
    uint8_t getZ() const;
    uint8_t getN() const;
    uint8_t getH() const;
    uint8_t getC() const;

    void setZ(bool on);
    void setN(bool on); // Antes setSubstraction
    void setH(bool on); // Antes setHalf
    void setC(bool on); // Antes setCarry

    // --- REGISTROS VIRTUALES 16 BITS ---
    uint16_t getAF() const; void setAF(uint16_t val);
    uint16_t getBC() const; void setBC(uint16_t val);
    uint16_t getDE() const; void setDE(uint16_t val);
    uint16_t getHL() const; void setHL(uint16_t val);

    // --- HELPERS DE LECTURA ---
    uint8_t fetch();                 // Trae el opcode y avanza PC
    uint8_t readImmediateByte();     // Lee d8
    uint16_t readImmediateWord();    // Lee d16
    void push(uint16_t value);
    uint16_t pop();

    // --- SISTEMA DE INSTRUCCIONES ---
    using Instruction = int(cpu::*)(uint8_t); // Puntero a función miembro
    
    // Game Boy tiene 256 opcodes posibles (0x00 a 0xFF)
    std::array<Instruction, 256> table_opcode; 

    // Instrucciones individuales
    int NOP(uint8_t opcode);
    int JP(uint8_t opcode);
};