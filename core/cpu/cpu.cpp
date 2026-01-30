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