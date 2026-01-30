#include "cpu.h"

// --- Constructor ---
cpu::cpu(mmu& mmu_ref) : memory(mmu_ref) 
{
    // 1. Estado Inicial
    IME = false;
    isStopped = false;
    isHalted = false;
    PC = 0x100;
    SP = 0xFFFE;
    r8.fill(0);

    // 2. Inicialización de la tabla
    table_opcode.fill(nullptr);

    // --- GRUPO: CARGAS DE 8 BITS (LD r8, r8) ---
    // Mapeamos el bloque 0x40 - 0x7F (Nota: la función maneja el caso 0x76 internamente)
    for (int i = 0x40; i <= 0x7F; i++) {
        table_opcode[i] = &cpu::LD_r8_r8;
    }

    // --- GRUPO: CARGAS DE 8 BITS (LD r8, d8) ---
    table_opcode[0x06] = &cpu::LD_r8_d8; // LD B, d8
    table_opcode[0x0E] = &cpu::LD_r8_d8; // LD C, d8
    table_opcode[0x16] = &cpu::LD_r8_d8; // LD D, d8
    table_opcode[0x1E] = &cpu::LD_r8_d8; // LD E, d8
    table_opcode[0x26] = &cpu::LD_r8_d8; // LD H, d8
    table_opcode[0x2E] = &cpu::LD_r8_d8; // LD L, d8
    table_opcode[0x36] = &cpu::LD_r8_d8; // LD [HL], d8
    table_opcode[0x3E] = &cpu::LD_r8_d8; // LD A, d8

    // --- GRUPO: CARGAS DE 16 BITS ---
    table_opcode[0x21] = &cpu::LD_r16_d16; // LD HL, d16
    table_opcode[0xF9] = &cpu::LD_SP_HL;   // LD SP, HL
    table_opcode[0x32] = &cpu::LDD_HL_A;    // LD (HL-), A (Carga y decrementa)

    // --- GRUPO: ARITMÉTICA Y LÓGICA ---
    table_opcode[0xAF] = &cpu::XOR_A;      // XOR A
    table_opcode[0x05] = &cpu::DEC_r8;     // DEC B
    table_opcode[0x0D] = &cpu::DEC_r8;     // DEC C
    table_opcode[0x15] = &cpu::DEC_r8;     // DEC D
    table_opcode[0x1D] = &cpu::DEC_r8;     // DEC E
    table_opcode[0x3D] = &cpu::DEC_r8;     // DEC A
    table_opcode[0x27] = &cpu::DAA;        // Ajuste decimal
    

    // --- GRUPO: SALTOS (JUMPS & BRANCHES) ---
    table_opcode[0xC3] = &cpu::JP;         // JP d16 (Absoluto)
    table_opcode[0x18] = &cpu::JR_r8;      // JR r8 (Relativo)
    table_opcode[0x20] = &cpu::JR_cc_r8;   // JR NZ, r8
    table_opcode[0x28] = &cpu::JR_cc_r8;   // JR Z, r8
    // ... en el constructor ...
    
    // --- GRUPO: HARDWARE I/O (High RAM) ---
    table_opcode[0xE0] = &cpu::LDH_n_A; // LDH [a8], A
    table_opcode[0xF0] = &cpu::LDH_A_n; // LDH A, [a8]

    // --- GRUPO: ROTACIONES ---
    table_opcode[0x0F] = &cpu::RRCA;    // RRCA

    // --- GRUPO: CONTROL DE HARDWARE ---
    table_opcode[0x00] = &cpu::NOP;
    table_opcode[0x10] = &cpu::STOP;
    table_opcode[0x76] = &cpu::HALT;
    table_opcode[0xF3] = &cpu::DI;         // Disable Interrupts
    table_opcode[0xFB] = &cpu::EI;         // Enable Interrupts
    // Control de Flujo
    table_opcode[0xCD] = &cpu::CALL;      // CALL a16
    table_opcode[0xCA] = &cpu::JP_cc_a16; // JP Z, a16
    
    // Lógica
    table_opcode[0xB6] = &cpu::OR_r8;     // OR [HL]

    // --- GRUPO: CARGAS DIRECTAS ---   
    table_opcode[0xEA] = &cpu::LD_a16_A;

    // ... En tu constructor cpu::cpu ...

    // REUTILIZACIÓN: Cargas de 16 bits (LD r16, d16)
    // Ya tenías la 0x21, ahora mapeamos las hermanas:
    table_opcode[0x01] = &cpu::LD_r16_d16; // LD BC, d16
    table_opcode[0x11] = &cpu::LD_r16_d16; // LD DE, d16
    
    // REUTILIZACIÓN: Saltos Relativos (JR cc, r8)
    // Ya tenías 0x20 y 0x28, ahora mapeamos Carry/NoCarry
    table_opcode[0x30] = &cpu::JR_cc_r8;   // JR NC, r8
    table_opcode[0x38] = &cpu::JR_cc_r8;   // JR C, r8  <-- EL QUE PEDÍA EL LOG

    // NUEVAS: Aritmética 16 bits
    table_opcode[0x0B] = &cpu::DEC_r16;    // DEC BC    <-- EL QUE PEDÍA EL LOG
    table_opcode[0x13] = &cpu::INC_r16;    // INC DE    <-- EL QUE PEDÍA EL LOG
    table_opcode[0x23] = &cpu::INC_r16;    // INC HL

    // NUEVAS: Memoria y Copia
    table_opcode[0x2A] = &cpu::LDI_A_HL;   // LD A, [HL+] (Carga y avanza)
    table_opcode[0x12] = &cpu::LD_DE_A;    // LD [DE], A

    // NUEVAS: Retorno
    table_opcode[0xC9] = &cpu::RET;        // RET       <-- CRÍTICO

    // NUEVAS: Aritmética 8 bits
    table_opcode[0x80] = &cpu::ADD_A_r8;   // ADD A, B
    table_opcode[0xB1] = &cpu::OR_r8;      // OR C

    // --- GRUPO: STACK & MEMORIA ---
    table_opcode[0x31] = &cpu::LD_SP_d16;  // LD SP, d16  <-- MUY IMPORTANTE
    table_opcode[0x1A] = &cpu::LD_A_DE;    // LD A, [DE]

    // --- GRUPO: ARITMÉTICA 16 BITS (ADD HL, ss) ---
    table_opcode[0x09] = &cpu::ADD_HL_r16; // ADD HL, BC
    table_opcode[0x19] = &cpu::ADD_HL_r16; // ADD HL, DE  <-- EL QUE TE FALTA
    table_opcode[0x29] = &cpu::ADD_HL_r16; // ADD HL, HL
    table_opcode[0x39] = &cpu::ADD_HL_r16; // ADD HL, SP

    // --- GRUPO: COMPARACIONES ---
    table_opcode[0xFE] = &cpu::CP_d8;      // CP d8       <-- CRÍTICO PARA LOGICA

    // --- GRUPO: RESTA (SUB r8) ---
    // Mapeamos todo el bloque 0x90-0x97
    table_opcode[0x90] = &cpu::SUB_r8; // SUB B
    table_opcode[0x91] = &cpu::SUB_r8; // SUB C
    table_opcode[0x92] = &cpu::SUB_r8; // SUB D
    table_opcode[0x93] = &cpu::SUB_r8; // SUB E
    table_opcode[0x94] = &cpu::SUB_r8; // SUB H       <-- EL QUE TE FALTA
    table_opcode[0x95] = &cpu::SUB_r8; // SUB L
    table_opcode[0x96] = &cpu::SUB_r8; // SUB [HL]
    table_opcode[0x97] = &cpu::SUB_r8; // SUB A

    // --- GRUPO: STACK OPS ---
    table_opcode[0xC5] = &cpu::PUSH_r16; // PUSH BC
    table_opcode[0xD5] = &cpu::PUSH_r16; // PUSH DE
    table_opcode[0xE5] = &cpu::PUSH_r16; // PUSH HL <-- EL QUE PIDE EL LOG
    table_opcode[0xF5] = &cpu::PUSH_r16; // PUSH AF <-- EL QUE PIDE EL LOG
    
    table_opcode[0xC1] = &cpu::POP_r16;  // POP BC
    table_opcode[0xD1] = &cpu::POP_r16;  // POP DE
    table_opcode[0xE1] = &cpu::POP_r16;  // POP HL
    table_opcode[0xF1] = &cpu::POP_r16;  // POP AF <-- EL QUE PIDE EL LOG

    table_opcode[0x08] = &cpu::LD_a16_SP; // LD [a16], SP <-- EL QUE PIDE EL LOG

    // --- GRUPO: PREFIJO CB ---
    table_opcode[0xCB] = &cpu::PREFIX_CB; // <-- EL JEFE FINAL

    // --- GRUPO: MATH CON CARRY ---
    table_opcode[0x89] = &cpu::ADC_A_r8;  // ADC A, C
    table_opcode[0x99] = &cpu::SBC_A_r8;  // SBC A, C
    table_opcode[0xDE] = &cpu::SBC_A_d8;  // SBC A, d8

    // --- GRUPO: AND (Lógica Y) ---
    table_opcode[0xA0] = &cpu::AND_r8; // AND B
    table_opcode[0xA1] = &cpu::AND_r8; // AND C
    table_opcode[0xA2] = &cpu::AND_r8; // AND D
    table_opcode[0xA3] = &cpu::AND_r8; // AND E
    table_opcode[0xA4] = &cpu::AND_r8; // AND H
    table_opcode[0xA5] = &cpu::AND_r8; // AND L
    table_opcode[0xA6] = &cpu::AND_r8; // AND [HL]
    table_opcode[0xA7] = &cpu::AND_r8; // AND A  <-- EL QUE PIDE EL LOG

    // --- GRUPO: RET CONDICIONAL ---
    table_opcode[0xC0] = &cpu::RET_cc; // RET NZ
    table_opcode[0xC8] = &cpu::RET_cc; // RET Z  <-- EL QUE PIDE EL LOG
    table_opcode[0xD0] = &cpu::RET_cc; // RET NC
    table_opcode[0xD8] = &cpu::RET_cc; // RET C
    // --- GRUPO: INC r8 (Incrementar registro 8 bits) ---
    table_opcode[0x04] = &cpu::INC_r8; // INC B
    table_opcode[0x0C] = &cpu::INC_r8; // INC C  <-- EL QUE PIDE EL LOG
    table_opcode[0x14] = &cpu::INC_r8; // INC D
    table_opcode[0x1C] = &cpu::INC_r8; // INC E  <-- EL QUE PIDE EL LOG
    table_opcode[0x24] = &cpu::INC_r8; // INC H  <-- EL QUE PIDE EL LOG
    table_opcode[0x2C] = &cpu::INC_r8; // INC L
    table_opcode[0x34] = &cpu::INC_r8; // INC [HL]
    table_opcode[0x3C] = &cpu::INC_r8; // INC A

    // --- Cargas y Lógica Variada ---
    table_opcode[0xE2] = &cpu::LD_C_A;   // LD [C], A (I/O Write) <-- MUY IMPORTANTE
    table_opcode[0xFA] = &cpu::LD_A_a16; // LD A, [a16]
    table_opcode[0xC6] = &cpu::ADD_A_d8; // ADD A, d8
    table_opcode[0x37] = &cpu::SCF;      // SCF

    // --- GRUPO: JUMP RELATIVE (JR) ---
    table_opcode[0x18] = &cpu::JR_d8;     // JR n
    table_opcode[0x20] = &cpu::JR_cc_d8;  // JR NZ, n
    table_opcode[0x28] = &cpu::JR_cc_d8;  // JR Z, n
    table_opcode[0x30] = &cpu::JR_cc_d8;  // JR NC, n
    table_opcode[0x38] = &cpu::JR_cc_d8;  // JR C, n
    // --- RETI ---
    table_opcode[0xD9] = &cpu::RETI;
    // --- LDI (Load and Increment) ---
    table_opcode[0x22] = &cpu::LDI_HL_A;
    table_opcode[0x22] = &cpu::LDI_HL_A;

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
    maskF();
}
void cpu::setN(bool on) { 
    if (on) r8[F] |= (1 << 6); 
    else    r8[F] &= ~(1 << 6);
    maskF(); 
}
void cpu::setH(bool on) { 
    if (on) r8[F] |= (1 << 5); 
    else    r8[F] &= ~(1 << 5); 
    maskF();
}
void cpu::setC(bool on) { 
    if (on) r8[F] |= (1 << 4); 
    else    r8[F] &= ~(1 << 4); 
    maskF();
}

// --- Registros 16-bits (Getters/Setters) ---
uint16_t cpu::getAF() const { 
    return (r8[A] << 8) | (r8[F] & 0xF0); 
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

// --- Gestión del Stack ---
void cpu::push(uint16_t value)
{
    uint8_t byte_high = (value >> 8) & 0xFF;
    uint8_t byte_low = value & 0xFF;

    SP--;
    memory.writeMemory(SP, byte_high);

    SP--;
    memory.writeMemory(SP, byte_low);
}

uint16_t cpu::pop()
{
    uint8_t byte_low = memory.readMemory(SP);
    SP++;

    uint8_t byte_high = memory.readMemory(SP);
    SP++;

    return (byte_high << 8) | byte_low;
}

// --- Helpers de Interrupciones ---
void cpu::requestInterrupt(int bit) {
    uint8_t if_reg = memory.readMemory(0xFF0F);
    if_reg |= (1 << bit);
    memory.writeMemory(0xFF0F, if_reg);
}

void cpu::executeInterrupt(int bit) {
    // 1. Limpiar el bit de la interrupción en IF (0xFF0F)
    uint8_t if_reg = memory.readMemory(0xFF0F);
    if_reg &= ~(1 << bit);
    memory.writeMemory(0xFF0F, if_reg);

    // 2. Deshabilitar interrupciones globales
    IME = false;

    // 3. Guardar el PC actual en el Stack
    push(PC);

    // 4. Saltar al vector correspondiente
    // Vectores: 0x40 (Vblank), 0x48 (LCD), 0x50 (Timer), 0x58 (Serial), 0x60 (Joypad)
    PC = 0x0040 + (bit * 8);
}

void cpu::handleInterrupts() {
    uint8_t if_reg = memory.readMemory(0xFF0F);
    uint8_t ie_reg = memory.readMemory(0xFFFF);

    // Interrupciones pendientes = Solicitadas (IF) AND Habilitadas (IE)
    uint8_t pending = if_reg & ie_reg;

    if (pending > 0) {
        // Despertar a la CPU si estaba dormida
        isHalted = false;
        isStopped = false;

        // Si el IME está activo, ejecutamos la interrupción
        if (IME) {
            // Buscamos la interrupción con mayor prioridad (bit más bajo)
            for (int i = 0; i < 5; i++) {
                if ((pending >> i) & 1) {
                    executeInterrupt(i);
                    break; // Solo atendemos una interrupción por ciclo
                }
            }
        }
    }
}

// --- Helper de Seguridad para Flags ---
void cpu::maskF()
{
    // Los 4 bits bajos del registro F siempre deben ser 0
    r8[F] &= 0xF0;
}

// --- Ciclo Principal (Step) ---
int cpu::step()
{
    // 1. Revisar interrupciones antes de ejecutar nada
    handleInterrupts();

    // 2. Si la CPU está parada, no ejecutamos instrucciones
    if (isHalted || isStopped) {
        return 4; // Consumimos ciclos mientras esperamos
    }

    // 3. Fetch (Traer instrucción)
    uint8_t opcode = fetch();

    // 4. Decode & Execute
    if (table_opcode[opcode] != nullptr)
    {
        return (this->*table_opcode[opcode])(opcode);
    } 
    else
    {
        std::cout << "Opcode no implementado: 0x" << std::hex << (int)opcode << "\n";
        return 0;
    }
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

int cpu::DAA(uint8_t opcode)
{
    (void)opcode;
    uint8_t adjustment = 0;
    bool carry = false;

    // --- PASO 1: Determinar el ajuste ---
    if (getN() == 0) { // Suma
        if (getC() || r8[A] > 0x99) {
            adjustment |= 0x60;
            carry = true;
        }
        if (getH() || (r8[A] & 0x0F) > 0x09) {
            adjustment |= 0x06;
        }
        r8[A] += adjustment;
    } 
    else { // Resta
        if (getC()) {
            adjustment |= 0x60;
            carry = true;
        }
        if (getH()) {
            adjustment |= 0x06;
        }
        r8[A] -= adjustment;
    }

    // --- PASO 2: Actualizar Banderas ---
    setZ(r8[A] == 0);
    setH(false);
    setC(carry);
    std::cout <<"DAA " << "\n";

    // NOTA: No hacemos PC++ aquí, fetch() ya lo hizo.
    return 4; 
}

int cpu::STOP(uint8_t opcode)
{
    (void)opcode;
    // STOP es 0x10 0x00, saltamos el byte extra
    PC++; 
    isStopped = true;
    std::cout <<"stop " << "\n";
    return 4;
}

int cpu::HALT(uint8_t opcode) {
    (void)opcode;
    isHalted = true;
    std::cout <<"halt " << "\n";
    return 4; 
}

int cpu::NOP(uint8_t opcode) {
    (void)opcode;
    std::cout <<"nop " << "\n";
    return 4;
}

int cpu::JP(uint8_t opcode) {
    (void)opcode;
    uint16_t targetAddress = readImmediateWord(); 
    PC = targetAddress;
    std::cout <<"jump " << "\n";
    return 16;
}

int cpu::DI(uint8_t opcode) {
    (void)opcode;
    IME = false;
    std::cout <<"di " << "\n";
    return 4;
}

int cpu::EI(uint8_t opcode) {
    (void)opcode;
    IME = true;
    std::cout <<"ei " << "\n";
    return 4;
}
//LOAD INTRUCTIONS
int cpu::LD_r8_r8(uint8_t opcode) {
    // 1. Extraer índices de hardware (3 bits cada uno)
    int destBits = (opcode >> 3) & 0x07;
    int srcBits = opcode & 0x07;

    // 2. Mapa: Hardware Code -> Tu enum R8
    // Hardware: 0=B, 1=C, 2=D, 3=E, 4=H, 5=L, 6=[HL], 7=A
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A}; 

    // 3. Caso especial: 0x76 es HALT, no LD [HL], [HL]
    if (opcode == 0x76) {
            std::cout <<"halt " << "\n";
        return HALT(opcode); 
    }

    // 4. Lógica de ejecución
    if (destBits == 6) { // LD [HL], r8
        uint16_t address = getHL();
        uint8_t value = r8[hardwareToYourEnum[srcBits]];
        memory.writeMemory(address, value);
            std::cout <<"LD [HL], r8 " << "\n";

        return 8; // Escribir en memoria toma más tiempo
    } 
    else if (srcBits == 6) { // LD r8, [HL]
        uint16_t address = getHL();
        uint8_t value = memory.readMemory(address);
        r8[hardwareToYourEnum[destBits]] = value;
         std::cout <<"LD r8, [HL] " << "\n";
        return 8; // Leer de memoria toma más tiempo
    } 
    else { // LD r8, r8 (Registro a Registro)
         std::cout <<"LD r8, r8 " << "\n";
        r8[hardwareToYourEnum[destBits]] = r8[hardwareToYourEnum[srcBits]];
        return 4; // Operación interna rápida
    }
}
int cpu::XOR_A(uint8_t opcode) {
    (void)opcode;
    
    // Operación: A = A XOR A (El resultado siempre es 0)
    r8[A] ^= r8[A];

    // Banderas:
    // Z (Zero) = 1 (Porque el resultado es 0)
    // N (Resta) = 0
    // H (Half Carry) = 0
    // C (Carry) = 0
    setZ(true);
    setN(false);
    setH(false);
    setC(false);

    std::cout << "XOR A\n"; 
    return 4; // Ciclos
}
int cpu::LD_r16_d16(uint8_t opcode) {
    // Patrón de bits: 00 rr 0001
    // rr: 00=BC, 01=DE, 10=HL, 11=SP
    uint8_t reg_index = (opcode >> 4) & 0x03;

    // Leemos el valor de 16 bits inmediato (Little Endian)
    uint16_t value = readImmediateWord();

    switch (reg_index) {
        case 0: setBC(value); break; // 0x01
        case 1: setDE(value); break; // 0x11
        case 2: setHL(value); break; // 0x21 (Tu caso actual)
        case 3: SP = value;   break; // 0x31
    }

     std::cout << "LD r16, d16 val=" << std::hex << value << "\n";
    return 12; // Ciclos
}
int cpu::LDD_HL_A(uint8_t opcode) {
    (void)opcode;
    
    // 1. Escribir A en la dirección (HL)
    uint16_t addr = getHL();
    memory.writeMemory(addr, r8[A]);

    // 2. Decrementar HL
    setHL(addr - 1);

     std::cout << "LD (HL-), A\n";
    return 8; // Ciclos
}
int cpu::LD_r8_d8(uint8_t opcode) {
    // 1. Leer el valor que sigue al opcode (el d8)
    uint8_t value = readImmediateByte();

    // 2. Identificar el registro destino usando los bits 3, 4 y 5
    int regBits = (opcode >> 3) & 0x07;

    // Mapa de hardware: 0=B, 1=C, 2=D, 3=E, 4=H, 5=L, 6=[HL], 7=A
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A}; 

    if (regBits == 6) { // Caso especial: LD [HL], d8
        memory.writeMemory(getHL(), value);
         std::cout << "LD [HL], " << (int)value << "\n";
        return 12; // Escribir en memoria es más lento
    } else {
        r8[hardwareToYourEnum[regBits]] = value;
        std::cout << "LD " << (int)regBits << ", " << (int)value << "\n";
        return 8; 
    }
}
int cpu::DEC_r8(uint8_t opcode) {
    int regBits = (opcode >> 3) & 0x07;
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A}; 
    R8 target = hardwareToYourEnum[regBits];

    uint8_t prevValue = r8[target];
    r8[target]--;

    // Banderas
    setZ(r8[target] == 0);
    setN(true); // Siempre true en decrementos
    // Half Carry: se activa si hubo préstamo del bit 4 (0x0F -> 0x10)
    setH((prevValue & 0x0F) == 0); 
    
    return (regBits == 6) ? 12 : 4; // Si es [HL] tarda más
}
int cpu::JR_cc_r8(uint8_t opcode) {
    // Leer el desplazamiento (signed!)
    int8_t offset = (int8_t)readImmediateByte(); 
    
    // Determinar la condición
    int condition = (opcode >> 3) & 0x03;
    bool jump = false;

    switch(condition) {
        case 0: jump = (getZ() == 0); break; // NZ
        case 1: jump = (getZ() == 1); break; // Z
        case 2: jump = (getC() == 0); break; // NC
        case 3: jump = (getC() == 1); break; // C
    }

    if (jump) {
        PC += offset;
        return 12; // Saltando tarda más
    }
    return 8;
}

int cpu::JR_r8(uint8_t opcode) {
    int8_t offset = (int8_t)readImmediateByte();
    PC += offset;
    return 12;
}
int cpu::LD_SP_HL(uint8_t opcode) {
    (void)opcode;
    SP = getHL();
    return 8;
}


int cpu::LDH_n_A(uint8_t opcode) {
    (void)opcode;
    // Lee el offset (ej: 0x40)
    uint8_t offset = readImmediateByte();
    
    // Escribe A en 0xFF00 + offset
    memory.writeMemory(0xFF00 | offset, r8[A]);
    
     std::cout << "LDH [FF" << std::hex << (int)offset << "], A\n";
    return 12;
}

int cpu::LDH_A_n(uint8_t opcode) {
    (void)opcode;
    uint8_t offset = readImmediateByte();
    
    // Lee de 0xFF00 + offset y guarda en A
    r8[A] = memory.readMemory(0xFF00 | offset);
    
    return 12;
}
int cpu::RRCA(uint8_t opcode) {
    (void)opcode;
    uint8_t bit0 = r8[A] & 0x01;
    
    // Rotar A a la derecha
    r8[A] = (r8[A] >> 1) | (bit0 << 7);
    
    // Banderas:
    // Z se pone a 0 en esta instrucción específica (aunque el resultado sea 0)
    setZ(false); 
    setN(false);
    setH(false);
    setC(bit0 == 1);
    
    return 4;
}
int cpu::LD_a16_A(uint8_t opcode) {
    (void)opcode;
    // 1. Leer la dirección de destino (2 bytes)
    uint16_t addr = readImmediateWord();
    
    // 2. Escribir A en esa dirección
    memory.writeMemory(addr, r8[A]);
    
    // std::cout << "LD [" << std::hex << addr << "], A\n";
    return 16; // 4 ciclos de reloj (16 ticks)
}
int cpu::CALL(uint8_t opcode) {
    (void)opcode;
    // 1. Leer la dirección de la subrutina (destino)
    uint16_t target = readImmediateWord();
    
    // 2. Empujar el PC actual (que ya apunta a la instrucción SIGUIENTE) al Stack
    push(PC);
    
    // 3. Saltar
    PC = target;
    
    // std::cout << "CALL " << std::hex << target << "\n";
    return 24; // Toma 6 ciclos (24 ticks)
}
int cpu::JP_cc_a16(uint8_t opcode) {
    uint16_t target = readImmediateWord();
    
    // Bits 3-4 definen la condición: 0=NZ, 1=Z, 2=NC, 3=C
    // 0xCA es Z (condition 1). 0xC2 es NZ (condition 0).
    int condition = (opcode >> 3) & 0x03;
    bool jump = false;

    switch(condition) {
        case 0: jump = !getZ(); break; // NZ
        case 1: jump = getZ();  break; // Z
        case 2: jump = !getC(); break; // NC
        case 3: jump = getC();  break; // C
    }

    if (jump) {
        PC = target;
        return 16;
    }
    return 12;
}
int cpu::OR_r8(uint8_t opcode) {
    // Determinar registro fuente
    int srcIndex = opcode & 0x07;
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    if (srcIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 8;
    } else {
        val = r8[hardwareToYourEnum[srcIndex]];
    }

    // Operación OR
    r8[A] |= val;

    // Banderas
    setZ(r8[A] == 0);
    setN(false);
    setH(false);
    setC(false);

    return cycles;
}
int cpu::RET(uint8_t opcode) {
    (void)opcode;
    PC = pop(); // Recuperar dirección de retorno
    // std::cout << "RET to " << std::hex << PC << "\n";
    return 16;
}
// Opcode 0x2A: Carga A desde [HL] y luego incrementa HL
int cpu::LDI_A_HL(uint8_t opcode) {
    (void)opcode;
    uint16_t hl = getHL();
    r8[A] = memory.readMemory(hl);
    setHL(hl + 1); // Incremento post-operación
    return 8;
}

// Opcode 0x12: Guarda A en la dirección apuntada por DE
int cpu::LD_DE_A(uint8_t opcode) {
    (void)opcode;
    memory.writeMemory(getDE(), r8[A]);
    return 8;
}
int cpu::INC_r16(uint8_t opcode) {
    // Bits 4-5: 00=BC, 01=DE, 10=HL, 11=SP
    int reg = (opcode >> 4) & 0x03;
    switch(reg) {
        case 0: setBC(getBC() + 1); break;
        case 1: setDE(getDE() + 1); break;
        case 2: setHL(getHL() + 1); break;
        case 3: SP++; break;
    }
    return 8;
}

int cpu::DEC_r16(uint8_t opcode) {
    int reg = (opcode >> 4) & 0x03;
    switch(reg) {
        case 0: setBC(getBC() - 1); break;
        case 1: setDE(getDE() - 1); break;
        case 2: setHL(getHL() - 1); break;
        case 3: SP--; break;
    }
    return 8;
}
int cpu::ADD_A_r8(uint8_t opcode) {
    int srcIndex = opcode & 0x07;
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val = (srcIndex == 6) ? memory.readMemory(getHL()) : r8[hardwareToYourEnum[srcIndex]];
    
    uint16_t result = r8[A] + val;
    
    // Banderas
    setZ((result & 0xFF) == 0);
    setN(false);
    // Half Carry: Si la suma de los 4 bits bajos supera 15
    setH(((r8[A] & 0x0F) + (val & 0x0F)) > 0x0F);
    // Carry: Si el resultado es mayor a 255
    setC(result > 0xFF);
    
    r8[A] = (uint8_t)result;
    return (srcIndex == 6) ? 8 : 4;
}
int cpu::LD_SP_d16(uint8_t opcode) {
    (void)opcode;
    SP = readImmediateWord();
    // std::cout << "LD SP, " << std::hex << SP << "\n";
    return 12;
}
int cpu::LD_A_DE(uint8_t opcode) {
    (void)opcode;
    r8[A] = memory.readMemory(getDE());
    return 8;
}
int cpu::ADD_HL_r16(uint8_t opcode) {
    int regIndex = (opcode >> 4) & 0x03;
    uint16_t val = 0;
    
    switch(regIndex) {
        case 0: val = getBC(); break;
        case 1: val = getDE(); break;
        case 2: val = getHL(); break;
        case 3: val = SP; break;
    }

    uint16_t hl = getHL();
    uint32_t result = hl + val;
    
    setHL((uint16_t)result);
    
    setN(false);
    // Half Carry 16-bit: (bits 0-11) + (bits 0-11) > 0xFFF
    setH((hl & 0xFFF) + (val & 0xFFF) > 0xFFF);
    // Carry: Si desborda 16 bits
    setC(result > 0xFFFF);
    
    return 8;
}
int cpu::CP_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t n = readImmediateByte();
    uint8_t a = r8[A];
    
    setZ(a == n);
    setN(true);
    setH((a & 0x0F) < (n & 0x0F)); // Préstamo del bit 4
    setC(a < n);                   // Préstamo total (A es menor que n)
    
    return 8;
}
int cpu::SUB_r8(uint8_t opcode) {
    int srcIndex = opcode & 0x07;
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    if (srcIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 8;
    } else {
        val = r8[hardwareToYourEnum[srcIndex]];
    }

    uint8_t a = r8[A];
    int result = a - val;
    
    r8[A] = (uint8_t)result;
    
    setZ(r8[A] == 0);
    setN(true);
    setH((a & 0x0F) < (val & 0x0F));
    setC(a < val);
    
    return cycles;
}

int cpu::PREFIX_CB(uint8_t opcode) {
    (void)opcode;
    // 1. LEER el siguiente byte (el verdadero opcode CB)
    uint8_t cb_op = readImmediateByte();
    
    // Decodificar operandos
    int regIndex = cb_op & 0x07; // Últimos 3 bits dicen el registro (B,C,D,E,H,L,HL,A)
    int bit = (cb_op >> 3) & 0x07; // Bits 3-5 dicen qué bit operar (0-7)
    int type = (cb_op >> 6) & 0x03; // Bits 6-7 dicen el tipo (Rotate, Bit, Res, Set)

    static const R8 map[] = {B, C, D, E, H, L, H, A}; // Nota: índice 6 es HL
    uint8_t val;
    int cycles = 8; // Ciclos base para registros

    // Leer valor
    if (regIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = (type == 1) ? 12 : 16; // BIT toma 12, otros 16 con [HL]
    } else {
        val = r8[map[regIndex]];
    }

    // --- EJECUTAR OPERACIÓN ---
    
    // TIPO 1: BIT (0x40 - 0x7F) - Testear si un bit es 0 o 1
    if (type == 1) { // Rango 0x40 - 0x7F
        bool isZero = (val & (1 << bit)) == 0;
        setZ(isZero);
        setN(false);
        setH(true);
        // Carry no cambia
        return cycles;
    }
    
    // SI NO ES "BIT", AÚN NO LO IMPLEMENTAMOS (Saldrá error en logs si se necesita)
    // Pero Tetris usa mayormente BIT al inicio.
    std::cout << "CB Opcode no implementado: " << std::hex << (int)cb_op << "\n";
    return 8;
}
int cpu::PUSH_r16(uint8_t opcode) {
    int reg = (opcode >> 4) & 0x03; // 00=BC, 01=DE, 10=HL, 11=AF
    uint16_t val = 0;
    
    switch(reg) {
        case 0: val = getBC(); break;
        case 1: val = getDE(); break;
        case 2: val = getHL(); break;
        case 3: val = getAF(); break;
    }
    
    push(val); // Tu función push ya debería restar SP y escribir
    return 16;
}

int cpu::POP_r16(uint8_t opcode) {
    int reg = (opcode >> 4) & 0x03;
    uint16_t val = pop(); // Tu función pop ya debería leer y sumar SP
    
    switch(reg) {
        case 0: setBC(val); break;
        case 1: setDE(val); break;
        case 2: setHL(val); break;
        case 3: 
            // IMPORTANTE: Limpiar los 4 bits bajos de F
            setAF(val & 0xFFF0); 
            break;
    }
    return 12;
}
int cpu::LD_a16_SP(uint8_t opcode) {
    (void)opcode;
    uint16_t addr = readImmediateWord();
    
    // Game Boy es Little Endian: primero low, luego high
    memory.writeMemory(addr, SP & 0xFF);
    memory.writeMemory(addr + 1, SP >> 8);
    
    return 20;
}
int cpu::ADC_A_r8(uint8_t opcode) {
    int src = opcode & 0x07;
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    uint8_t val = (src == 6) ? memory.readMemory(getHL()) : r8[map[src]];

    int carry = getC() ? 1 : 0;
    int result = r8[A] + val + carry;

    setZ((result & 0xFF) == 0);
    setN(false);
    // Half Carry: check nibble overflow
    setH(((r8[A] & 0x0F) + (val & 0x0F) + carry) > 0x0F);
    setC(result > 0xFF);

    r8[A] = (uint8_t)result;
    return (src == 6) ? 8 : 4;
}

int cpu::SBC_A_r8(uint8_t opcode) {
    int src = opcode & 0x07;
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    uint8_t val = (src == 6) ? memory.readMemory(getHL()) : r8[map[src]];

    int carry = getC() ? 1 : 0;
    int result = r8[A] - val - carry;

    setZ((result & 0xFF) == 0);
    setN(true);
    // Half Carry: check nibble borrow
    setH(((r8[A] & 0x0F) - (val & 0x0F) - carry) < 0);
    setC(result < 0);

    r8[A] = (uint8_t)result;
    return (src == 6) ? 8 : 4;
}

int cpu::SBC_A_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte();
    int carry = getC() ? 1 : 0;
    int result = r8[A] - val - carry;

    setZ((result & 0xFF) == 0);
    setN(true);
    setH(((r8[A] & 0x0F) - (val & 0x0F) - carry) < 0);
    setC(result < 0);

    r8[A] = (uint8_t)result;
    return 8;
}
int cpu::AND_r8(uint8_t opcode) {
    int srcIndex = opcode & 0x07;
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    if (srcIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 8;
    } else {
        val = r8[map[srcIndex]];
    }

    // Operación AND
    r8[A] &= val;

    // Banderas
    setZ(r8[A] == 0);
    setN(false);
    setH(true); // ¡IMPORTANTE! En Game Boy, AND pone H a 1.
    setC(false);

    return cycles;
}
int cpu::RET_cc(uint8_t opcode) {
    // Bits 3-4 indican la condición: 0=NZ, 1=Z, 2=NC, 3=C
    // Opcode: 11 cc 000
    int condition = (opcode >> 3) & 0x03;
    bool doReturn = false;

    switch(condition) {
        case 0: doReturn = !getZ(); break; // NZ (No Zero)
        case 1: doReturn = getZ();  break; // Z (Zero)
        case 2: doReturn = !getC(); break; // NC (No Carry)
        case 3: doReturn = getC();  break; // C (Carry)
    }

    if (doReturn) {
        PC = pop(); // Sacar dirección de retorno del Stack
        // std::cout << "RET cc Taken to " << std::hex << PC << "\n";
        return 20; // 5 ciclos si se cumple
    }

    // Si no se cumple, no hacemos nada
    return 8; // 2 ciclos si no se cumple
}
int cpu::INC_r8(uint8_t opcode) {
    int index = (opcode >> 3) & 0x07; // Bits 3-5 dicen el registro
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    // Obtener valor actual
    if (index == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 12;
    } else {
        val = r8[map[index]];
    }

    uint8_t result = val + 1;

    // Guardar valor
    if (index == 6) {
        memory.writeMemory(getHL(), result);
    } else {
        r8[map[index]] = result;
    }

    // Banderas
    setZ(result == 0);
    setN(false);
    // Half Carry: Si pasamos de 0x0F a 0x10 (bits bajos 1111 -> 0000)
    setH((val & 0x0F) == 0x0F);
    // Carry NO se toca en INC

    return cycles;
}
int cpu::LD_C_A(uint8_t opcode) {
    (void)opcode;
    // Dirección base de IO (0xFF00) + Registro C
    uint16_t addr = 0xFF00 | r8[C];
    
    memory.writeMemory(addr, r8[A]);
    
    // std::cout << "LD [FF" << std::hex << (int)r8[C] << "], A\n";
    return 8;
}
int cpu::LD_A_a16(uint8_t opcode) {
    (void)opcode;
    uint16_t addr = readImmediateWord();
    r8[A] = memory.readMemory(addr);
    return 16;
}
int cpu::ADD_A_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte();
    uint16_t result = r8[A] + val;

    setZ((result & 0xFF) == 0);
    setN(false);
    setH(((r8[A] & 0x0F) + (val & 0x0F)) > 0x0F);
    setC(result > 0xFF);

    r8[A] = (uint8_t)result;
    return 8;
}
int cpu::SCF(uint8_t opcode) {
    (void)opcode;
    setN(false);
    setH(false);
    setC(true);
    return 4;
}
int cpu::JR_d8(uint8_t opcode) {
    (void)opcode;
    // Leemos un byte con SIGNO (int8_t).
    // Si es 0xFE (-2), el PC retrocede 2 pasos.
    int8_t offset = (int8_t)readImmediateByte();
    PC += offset;
    return 12;
}

int cpu::JR_cc_d8(uint8_t opcode) {
    int8_t offset = (int8_t)readImmediateByte();
    
    // Decodificar condición (Bits 3-4): 0=NZ, 1=Z, 2=NC, 3=C
    int condition = (opcode >> 3) & 0x03;
    bool jump = false;

    switch(condition) {
        case 0: jump = !getZ(); break; // NZ
        case 1: jump = getZ();  break; // Z
        case 2: jump = !getC(); break; // NC
        case 3: jump = getC();  break; // C
    }

    if (jump) {
        PC += offset;
        return 12; // 3 ciclos de máquina si salta
    }
    
    return 8; // 2 ciclos si no salta
}
int cpu::RETI(uint8_t opcode) {
    (void)opcode;
    
    // 1. POP PC (Recuperar dirección de retorno de la Pila)
    // Leemos byte bajo
    uint8_t lo = memory.readMemory(SP);
    SP++;
    
    // Leemos byte alto
    uint8_t hi = memory.readMemory(SP);
    SP++;
    
    // Combinamos para formar el PC
    PC = (hi << 8) | lo;

    // 2. Habilitar Interrupciones inmediatamente
    IME = true; 

    return 16; // Tarda 16 ciclos
}
int cpu::LDI_HL_A(uint8_t opcode) {
    (void)opcode; // Evitar warning de variable no usada

    // 1. Obtenemos la dirección de memoria desde HL
    uint16_t addr = getHL();

    // 2. Escribimos el contenido del registro A en esa dirección
    memory.writeMemory(addr, r8[A]);

    // 3. Incrementamos HL (HL = HL + 1)
    setHL(addr + 1);

    return 8; // 8 ciclos
}