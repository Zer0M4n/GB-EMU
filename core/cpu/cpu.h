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
    // --- Cargas 16 bits ---
    int LD_SP_d16(uint8_t opcode); // 0x31
    int LD_A_DE(uint8_t opcode);   // 0x1A (Cargar A desde [DE])

    // --- Aritmética 16 bits ---
    int ADD_HL_r16(uint8_t opcode);// 0x09, 0x19, 0x29, 0x39

    // --- Aritmética 8 bits ---
    int CP_d8(uint8_t opcode);     // 0xFE (Comparar inmediato)
    int SUB_r8(uint8_t opcode);    // 0x90 - 0x97 (Resta)

    // --- PREFIJO CB ---
    int PREFIX_CB(uint8_t opcode); // El manejador del 0xCB

    // --- STACK (PUSH / POP) ---
    int PUSH_r16(uint8_t opcode);  // 0xC5, 0xD5, 0xE5, 0xF5
    int POP_r16(uint8_t opcode);   // 0xC1, 0xD1, 0xE1, 0xF1
    int LD_a16_SP(uint8_t opcode); // 0x08 (Guardar SP en memoria)

    // --- ARITMÉTICA CON CARRY (ADC / SBC) ---
    int ADC_A_r8(uint8_t opcode);  // 0x88 - 0x8F
    int SBC_A_r8(uint8_t opcode);  // 0x98 - 0x9F
    int SBC_A_d8(uint8_t opcode);  // 0xDE

    // --- LÓGICA 8 BITS ---
    int AND_r8(uint8_t opcode);  // 0xA0 - 0xA7

    // --- CONTROL DE FLUJO CONDICIONAL ---
    int RET_cc(uint8_t opcode);  // 0xC0 (NZ), 0xC8 (Z), 0xD0 (NC), 0xD8 (C)
    // --- ARITMÉTICA 8 BITS ---
    int INC_r8(uint8_t opcode);  // 0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C
    int ADD_A_d8(uint8_t opcode);// 0xC6 (Suma inmediata)

    // --- CARGAS ESPECIALES ---
    int LD_C_A(uint8_t opcode);  // 0xE2 (Escribir en I/O usando C: LD [0xFF00+C], A)
    int LD_A_a16(uint8_t opcode);// 0xFA (Leer A desde memoria absoluta: LD A, [a16])

    // --- BANDERAS ---
    int SCF(uint8_t opcode);     // 0x37 (Set Carry Flag)
    // --- SALTOS RELATIVOS ---
    int JR_d8(uint8_t opcode);     // 0x18 (Incondicional)
    int JR_cc_d8(uint8_t opcode);  // 0x20, 0x28, 0x30, 0x38 (Condicionales)
    // --- CONTROL DE FLUJO ---
    int RETI(uint8_t opcode); // 0xD9 Return from Interrupt
    // --- LOAD INCREMENT/DECREMENT ---
    int LDI_HL_A(uint8_t opcode); // 0x22: LD (HL+), A
};
