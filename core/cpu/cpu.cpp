#include "cpu.h"

// --- Constructor ---
cpu::cpu(mmu& mmu_ref) : memory(mmu_ref) 
{
    IME = false;
    isStopped = false;
    isHalted = false;
    PC = 0x100;   // Punto de entrada estándar
    SP = 0xFFFE;  // Final de la HRAM
    
    // Inicializar registros a 0
    r8.fill(0);

    // Inicializar tabla de opcodes con nullptr
    table_opcode.fill(nullptr);

    // Mapear instrucciones implementadas
    table_opcode[0x00] = &cpu::NOP;
    table_opcode[0x06] = &cpu::LD_r8_d8; // LD B, d8
    table_opcode[0x0E] = &cpu::LD_r8_d8; // LD C, d8  <-- ESTE ES EL QUE TE FALTA
    table_opcode[0x16] = &cpu::LD_r8_d8; // LD D, d8
    table_opcode[0x1E] = &cpu::LD_r8_d8; // LD E, d8
    table_opcode[0x26] = &cpu::LD_r8_d8; // LD H, d8
    table_opcode[0x2E] = &cpu::LD_r8_d8; // LD L, d8
    table_opcode[0x36] = &cpu::LD_r8_d8; // LD [HL], d8
    table_opcode[0x3E] = &cpu::LD_r8_d8; // LD A, d8
    table_opcode[0x10] = &cpu::STOP;

    table_opcode[0x21] = &cpu::LD_r16_d16; // LD HL, d16
    table_opcode[0x32] = &cpu::LDD_HL_A;   // LD (HL-), A
    table_opcode[0xAF] = &cpu::XOR_A;      // XOR A

    table_opcode[0x27] = &cpu::DAA;
    table_opcode[0x76] = &cpu::HALT;
    table_opcode[0xC3] = &cpu::JP;
    table_opcode[0xF3] = &cpu::DI;
    table_opcode[0xFB] = &cpu::EI;

    for (int i = 0x40; i <= 0x7F; i++) {
        table_opcode[i] = &cpu::LD_r8_r8;
    }
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