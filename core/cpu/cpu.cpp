#include "cpu.h"

// --- Constructor ---
cpu::cpu(mmu& mmu_ref) : memory(mmu_ref) 
{
    PC = 0x100; // Punto de entrada estándar para juegos de GB
    SP = 0xFFFE; // El stack suele empezar al final de la HRAM
    
    // Inicializar registros a 0 (o valores típicos de boot)
    r8.fill(0);

    // Inicializar tabla de opcodes con nullptr
    table_opcode.fill(nullptr);

    // Mapear instrucciones implementadas
    table_opcode[0x00] = &cpu::NOP;
    table_opcode[0xC3] = &cpu::JP;
}
void cpu::push(uint16_t value)
{
    uint8_t byte_hight = (value >> 8) & 0xFF;
    uint8_t byte_low = value & 0xFF;

    SP--;
    memory.writeMemory(SP , byte_hight);

    SP--;
    memory.writeMemory(SP , byte_low);
}
uint16_t cpu::pop()
{
    uint8_t byte_low = memory.readMemory(SP);
    SP++;

    uint8_t byte_hight = memory.readMemory(SP);
    SP++;

    return (byte_hight << 8) | byte_low;
}

// --- Ciclo Principal (Step) ---
int cpu::step()
{
    uint8_t opcode = fetch();
    int cycle;

    // Decodificar y Ejecutar
    if (table_opcode[opcode] != nullptr)
    {
        return (this->*table_opcode[opcode])(opcode);
    } 
    else
    {
        std::cout << "Opcode no implementado: 0x" << std::hex << (int)opcode << "\n";
        return 0;
    }
    return 0;
}

// --- Helpers de Lectura ---
uint8_t cpu::fetch() {
    uint8_t opcode = memory.readMemory(PC);
    PC++;
    return opcode;
}

uint8_t cpu::readImmediateByte() {
    uint8_t value = memory.readMemory(PC);
    PC++; 
    return value;
}

uint16_t cpu::readImmediateWord() {
    uint8_t low = readImmediateByte();
    uint8_t high = readImmediateByte();
    return (high << 8) | low;
}

// --- Instrucciones ---
int cpu::NOP(uint8_t opcode) {
    (void)opcode; // Evita warning de variable no usada
     std::cout << "DEBUG: Nop: 0x" << std::hex << PC << "\n";
     return 4;
}
int cpu::JP(uint8_t opcode) {
    (void)opcode;
    // JP lee los siguientes 2 bytes y salta a esa dirección
    uint16_t targetAddress = readImmediateWord(); 
    PC = targetAddress;
    std::cout << "DEBUG: Salto JP realizado a: 0x" << std::hex << PC << "\n";
    return 16;
}

// --- FLAGS (Getters) ---
uint8_t cpu::getZ() const { return (r8[F] >> 7) & 1; } 
uint8_t cpu::getN() const { return (r8[F] >> 6) & 1; }
uint8_t cpu::getH() const { return (r8[F] >> 5) & 1; }
uint8_t cpu::getC() const { return (r8[F] >> 4) & 1; }

// --- FLAGS (Setters) ---
void cpu::setZ(bool on) { 
    if (on) r8[F] |= (1 << 7); 
    else    r8[F] &= ~(1 << 7); 
}
void cpu::setN(bool on) { 
    if (on) r8[F] |= (1 << 6); 
    else    r8[F] &= ~(1 << 6); 
}
void cpu::setH(bool on) { 
    if (on) r8[F] |= (1 << 5); 
    else    r8[F] &= ~(1 << 5); 
}
void cpu::setC(bool on) { 
    if (on) r8[F] |= (1 << 4); 
    else    r8[F] &= ~(1 << 4); 
}

// --- Registros 16-bits (Getters/Setters) ---
uint16_t cpu::getAF() const { 
    return (r8[A] << 8) | (r8[F] & 0xF0); // Los 4 bits bajos de F siempre son 0
}
void cpu::setAF(uint16_t val) { 
    r8[A] = val >> 8; 
    r8[F] = val & 0xF0; 
}

uint16_t cpu::getBC() const { return (r8[B] << 8) | r8[C]; }
void cpu::setBC(const uint16_t val) {
    r8[B] = val >> 8;
    r8[C] = val & 0xFF;
}

uint16_t cpu::getDE() const { return (r8[D] << 8) | r8[E]; }
void cpu::setDE(const uint16_t val) {
    r8[D] = val >> 8;
    r8[E] = val & 0xFF;
}

uint16_t cpu::getHL() const { return (r8[H] << 8) | r8[L]; }
void cpu::setHL(const uint16_t val) {
    r8[H] = val >> 8;
    r8[L] = val & 0xFF;
}