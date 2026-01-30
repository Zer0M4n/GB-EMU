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
    int LD_r8_d8(uint8_t opcode);
    // ALU (Aritmética/Lógica)
    int XOR_A(uint8_t opcode);

    // Cargas de 16 bits (Cubre LD BC,d16; LD DE,d16; LD HL,d16; LD SP,d16)
    int LD_r16_d16(uint8_t opcode);

    // Cargas especiales de memoria (LD (HL-), A y LD (HL+), A)
    int LDD_HL_A(uint8_t opcode); // 0x32

    // Aritmética 8 bits
    int DEC_r8(uint8_t opcode); // 0x05, 0x0D, etc.

    // Saltos Relativos
    int JR_cc_r8(uint8_t opcode); // 0x20 (JR NZ), 0x28 (JR Z), 0x30 (JR NC), 0x38 (JR C)
    int JR_r8(uint8_t opcode);    // 0x18 (Salto incondicional)

    // Especiales
    int LD_SP_HL(uint8_t opcode); // 0xF9

    // --- NUEVAS EN PRIVATE ---
    // High RAM (Acceso rápido a 0xFFxx)
    int LDH_n_A(uint8_t opcode); // 0xE0: Escribir en IO
    int LDH_A_n(uint8_t opcode); // 0xF0: Leer de IO
    
    // Rotaciones (Posible causa del 0x0F si es real)
    int RRCA(uint8_t opcode);    // 0x0F

    int LD_a16_A(uint8_t opcode); // 0xEA
    int CALL(uint8_t opcode);    // 0xCD
    int JP_cc_a16(uint8_t opcode); // 0xCA (JP Z), 0xC2 (JP NZ), etc.
    int OR_r8(uint8_t opcode);   // 0xB6 (OR [HL]) y otros OR

    // --- CONTROL DE FLUJO ---
    int RET(uint8_t opcode);     // 0xC9 (Retorno de subrutina)

    // --- ARITMÉTICA 8 BITS ---
    int ADD_A_r8(uint8_t opcode);// 0x80 - 0x87 (Suma)

    // --- ARITMÉTICA 16 BITS ---
    int INC_r16(uint8_t opcode); // 0x03, 0x13, 0x23 (Incrementar par de registros)
    int DEC_r16(uint8_t opcode); // 0x0B, 0x1B, 0x2B (Decrementar par de registros)

    // --- CARGAS ESPECIALES ---
    int LDI_A_HL(uint8_t opcode);// 0x2A (LD A, [HL+]) Carga y aumenta HL
    int LD_DE_A(uint8_t opcode); // 0x12 (LD [DE], A)
};