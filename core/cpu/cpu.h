#pragma once

#include <array>
#include <cstdint>
#include <iostream>

// Asegúrate de que la ruta sea la correcta en tu proyecto
#include "mmu/mmu.h" 

class cpu
{
public:
    // Constructor
    cpu(mmu& mmu_ref);

    // Función principal para avanzar un paso (Fetch-Decode-Execute)
    int step();

    // Permite que componentes externos (Timer, PPU, Joypad) soliciten una interrupción
    void requestInterrupt(int bit);

private:
    // Referencia a la memoria (MMU)
    mmu& memory;

    // --- Estado de la CPU ---
    uint16_t PC; // Program Counter
    uint16_t SP; // Stack Pointer
    
    // Interrupt Master Enable
    bool IME; 
    
    // Estados de bajo consumo
    bool isHalted;
    bool isStopped;

    // Registros de 8 bits
    std::array<uint8_t, 8> r8; 

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
    void setN(bool on); 
    void setH(bool on); 
    void setC(bool on); 

    // --- REGISTROS VIRTUALES 16 BITS ---
    uint16_t getAF() const; void setAF(uint16_t val);
    uint16_t getBC() const; void setBC(uint16_t val);
    uint16_t getDE() const; void setDE(uint16_t val);
    uint16_t getHL() const; void setHL(uint16_t val);

    // --- HELPERS INTERNOS ---
    uint8_t fetch();                 // Trae el opcode y avanza PC
    uint8_t readImmediateByte();     // Lee siguiente byte (d8)
    uint16_t readImmediateWord();    // Lee siguiente word (d16)
    
    void push(uint16_t value);       // Empuja al Stack
    uint16_t pop();                  // Saca del Stack
    
    void maskF();                    // Asegura que los bits bajos de F sean 0
    
    // Gestión de Interrupciones
    void handleInterrupts();         // Revisa y gestiona interrupciones pendientes
    void executeInterrupt(int bit);  // Realiza el salto al vector de interrupción

    // --- SISTEMA DE INSTRUCCIONES ---
    using Instruction = int(cpu::*)(uint8_t); // Puntero a función miembro
    
    // Tabla de opcodes (256 instrucciones posibles)
    std::array<Instruction, 256> table_opcode; 

    // Instrucciones Misceláneas y de Control
    int NOP(uint8_t opcode);
    int STOP(uint8_t opcode);
    int DAA(uint8_t opcode);
    int HALT(uint8_t opcode);
    int DI(uint8_t opcode);
    int EI(uint8_t opcode);
    
    // Saltos
    int JP(uint8_t opcode);

    int LD_r8_r8(uint8_t opcode);
};