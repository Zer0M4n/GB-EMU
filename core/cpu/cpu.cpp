#include "cpu.h"
#include <iomanip>

// Debug simple de interrupciones en CPU (desactivado para producción)
static bool cpu_irq_debug_enabled = false;

// DEBUG: Rastrear opcodes ejecutados
static int opcode_counts[256] = {0};
static int total_instructions = 0;
static int last_dump_instructions = 0;

// --- Constructor ---
cpu::cpu(mmu& mmu_ref) : memory(mmu_ref) 
{
    // 1. Estado Inicial
    IME = false;
    isStopped = false;
    isHalted = false;
    IME_scheduled = false;

    PC = 0x100;
    SP = 0xFFFE;
    r8.fill(0);

    // 2. Inicialización de la tabla (Limpiar todo a nullptr)
    table_opcode.fill(nullptr);

    // =============================================================
    // GRUPO: CARGAS DE 8 BITS (LD)
    // =============================================================
    
    // LD r8, r8 (0x40 - 0x7F)
    for (int i = 0x40; i <= 0x7F; i++) {
        table_opcode[i] = &cpu::LD_r8_r8;
    }

    // LD r8, d8 (Inmediatos)
    table_opcode[0x06] = &cpu::LD_r8_d8; // LD B, d8
    table_opcode[0x0E] = &cpu::LD_r8_d8; // LD C, d8
    table_opcode[0x16] = &cpu::LD_r8_d8; // LD D, d8
    table_opcode[0x1E] = &cpu::LD_r8_d8; // LD E, d8
    table_opcode[0x26] = &cpu::LD_r8_d8; // LD H, d8
    table_opcode[0x2E] = &cpu::LD_r8_d8; // LD L, d8
    table_opcode[0x36] = &cpu::LD_r8_d8; // LD [HL], d8
    table_opcode[0x3E] = &cpu::LD_r8_d8; // LD A, d8

    // LD Especiales y Memoria
    table_opcode[0x0A] = &cpu::LD_A_BC;  // LD A, [BC]
    table_opcode[0x1A] = &cpu::LD_A_DE;  // LD A, [DE]
    table_opcode[0xFA] = &cpu::LD_A_a16; // LD A, [a16]
    table_opcode[0x02] = &cpu::LD_BC_A;  // LD [BC], A
    table_opcode[0x12] = &cpu::LD_DE_A;  // LD [DE], A
    table_opcode[0xEA] = &cpu::LD_a16_A; // LD [a16], A
    table_opcode[0xE0] = &cpu::LDH_n_A;  // LDH [a8], A
    table_opcode[0xF0] = &cpu::LDH_A_n;  // LDH A, [a8]
    table_opcode[0xE2] = &cpu::LD_C_A;   // LD [C], A
    // Esta es la lectura de High RAM usando C como offset
    table_opcode[0xF2] = &cpu::LD_A_C; // LD A, [C]  (Opcode 0xF2)
    // (Nota: LD A, [C] sería 0xF2, pero no es crítico ahora)

    // LDI / LDD (Incremento/Decremento HL)
    table_opcode[0x22] = &cpu::LDI_HL_A; // LD [HL+], A
    table_opcode[0x2A] = &cpu::LDI_A_HL; // LD A, [HL+]
    table_opcode[0x32] = &cpu::LDD_HL_A; // LD [HL-], A
    table_opcode[0x3A] = &cpu::LDD_A_HL; // LD A, [HL-]

    // =============================================================
    // GRUPO: 16 BITS (LD, INC, DEC, ADD)
    // =============================================================

    // LD r16, d16
    table_opcode[0x01] = &cpu::LD_r16_d16; // LD BC, d16
    table_opcode[0x11] = &cpu::LD_r16_d16; // LD DE, d16
    table_opcode[0x21] = &cpu::LD_r16_d16; // LD HL, d16
    table_opcode[0x31] = &cpu::LD_SP_d16;  // LD SP, d16

    // Stack (PUSH/POP)
    table_opcode[0xC5] = &cpu::PUSH_r16; // PUSH BC
    table_opcode[0xD5] = &cpu::PUSH_r16; // PUSH DE
    table_opcode[0xE5] = &cpu::PUSH_r16; // PUSH HL
    table_opcode[0xF5] = &cpu::PUSH_r16; // PUSH AF
    table_opcode[0xC1] = &cpu::POP_r16;  // POP BC
    table_opcode[0xD1] = &cpu::POP_r16;  // POP DE
    table_opcode[0xE1] = &cpu::POP_r16;  // POP HL
    table_opcode[0xF1] = &cpu::POP_r16;  // POP AF
    
    // LD SP
    table_opcode[0xF9] = &cpu::LD_SP_HL;  // LD SP, HL
    table_opcode[0x08] = &cpu::LD_a16_SP; // LD [a16], SP

    // INC r16 (Incrementos de 16 bits)
    table_opcode[0x03] = &cpu::INC_r16; // INC BC
    table_opcode[0x13] = &cpu::INC_r16; // INC DE
    table_opcode[0x23] = &cpu::INC_r16; // INC HL
    table_opcode[0x33] = &cpu::INC_r16; // INC SP

    // DEC r16 (Decrementos de 16 bits)
    table_opcode[0x0B] = &cpu::DEC_r16; // DEC BC
    table_opcode[0x1B] = &cpu::DEC_r16; // DEC DE
    table_opcode[0x2B] = &cpu::DEC_r16; // DEC HL
    table_opcode[0x3B] = &cpu::DEC_r16; // DEC SP

    // ADD HL, r16
    table_opcode[0x09] = &cpu::ADD_HL_r16; // ADD HL, BC
    table_opcode[0x19] = &cpu::ADD_HL_r16; // ADD HL, DE
    table_opcode[0x29] = &cpu::ADD_HL_r16; // ADD HL, HL
    table_opcode[0x39] = &cpu::ADD_HL_r16; // ADD HL, SP

    // =============================================================
    // GRUPO: ARITMÉTICA Y LÓGICA 8 BITS (ALU)
    // =============================================================

    // INC r8
    table_opcode[0x04] = &cpu::INC_r8; // INC B
    table_opcode[0x0C] = &cpu::INC_r8; // INC C
    table_opcode[0x14] = &cpu::INC_r8; // INC D
    table_opcode[0x1C] = &cpu::INC_r8; // INC E
    table_opcode[0x24] = &cpu::INC_r8; // INC H
    table_opcode[0x2C] = &cpu::INC_r8; // INC L
    table_opcode[0x34] = &cpu::INC_r8; // INC [HL]
    table_opcode[0x3C] = &cpu::INC_r8; // INC A

    // DEC r8
    table_opcode[0x05] = &cpu::DEC_r8; // DEC B
    table_opcode[0x0D] = &cpu::DEC_r8; // DEC C
    table_opcode[0x15] = &cpu::DEC_r8; // DEC D
    table_opcode[0x1D] = &cpu::DEC_r8; // DEC E
    table_opcode[0x25] = &cpu::DEC_r8; // DEC H
    table_opcode[0x2D] = &cpu::DEC_r8; // DEC L
    table_opcode[0x35] = &cpu::DEC_r8; // DEC [HL]
    table_opcode[0x3D] = &cpu::DEC_r8; // DEC A

    // ADD A, r8 (0x80 - 0x87) ---> AQUÍ ESTÁ EL 0x87 QUE FALTABA
    table_opcode[0x80] = &cpu::ADD_A_r8;
    table_opcode[0x81] = &cpu::ADD_A_r8;
    table_opcode[0x82] = &cpu::ADD_A_r8;
    table_opcode[0x83] = &cpu::ADD_A_r8;
    table_opcode[0x84] = &cpu::ADD_A_r8;
    table_opcode[0x85] = &cpu::ADD_A_r8;
    table_opcode[0x86] = &cpu::ADD_A_r8;
    table_opcode[0x87] = &cpu::ADD_A_r8; // ADD A, A

    // ADC A, r8 (0x88 - 0x8F) ---> FALTABAN ALGUNOS
    table_opcode[0x88] = &cpu::ADC_A_r8;
    table_opcode[0x89] = &cpu::ADC_A_r8;
    table_opcode[0x8A] = &cpu::ADC_A_r8;
    table_opcode[0x8B] = &cpu::ADC_A_r8;
    table_opcode[0x8C] = &cpu::ADC_A_r8; // ADC A, H
    table_opcode[0x8D] = &cpu::ADC_A_r8;
    table_opcode[0x8E] = &cpu::ADC_A_r8;
    table_opcode[0x8F] = &cpu::ADC_A_r8;

    // SUB r8 (0x90 - 0x97)
    table_opcode[0x90] = &cpu::SUB_r8;
    table_opcode[0x91] = &cpu::SUB_r8;
    table_opcode[0x92] = &cpu::SUB_r8;
    table_opcode[0x93] = &cpu::SUB_r8;
    table_opcode[0x94] = &cpu::SUB_r8; // SUB H
    table_opcode[0x95] = &cpu::SUB_r8;
    table_opcode[0x96] = &cpu::SUB_r8;
    table_opcode[0x97] = &cpu::SUB_r8;

    // SBC A, r8 (0x98 - 0x9F)
    table_opcode[0x98] = &cpu::SBC_A_r8;
    table_opcode[0x99] = &cpu::SBC_A_r8;
    table_opcode[0x9A] = &cpu::SBC_A_r8;
    table_opcode[0x9B] = &cpu::SBC_A_r8;
    table_opcode[0x9C] = &cpu::SBC_A_r8;
    table_opcode[0x9D] = &cpu::SBC_A_r8;
    table_opcode[0x9E] = &cpu::SBC_A_r8;
    table_opcode[0x9F] = &cpu::SBC_A_r8;

    // AND r8 (0xA0 - 0xA7)
    table_opcode[0xA0] = &cpu::AND_r8;
    table_opcode[0xA1] = &cpu::AND_r8;
    table_opcode[0xA2] = &cpu::AND_r8;
    table_opcode[0xA3] = &cpu::AND_r8;
    table_opcode[0xA4] = &cpu::AND_r8;
    table_opcode[0xA5] = &cpu::AND_r8;
    table_opcode[0xA6] = &cpu::AND_r8;
    table_opcode[0xA7] = &cpu::AND_r8;

    // XOR r8 (0xA8 - 0xAF) ---> AQUÍ ESTABAN FALTANDO
    table_opcode[0xA8] = &cpu::XOR_r8;
    table_opcode[0xA9] = &cpu::XOR_r8; // XOR C
    table_opcode[0xAA] = &cpu::XOR_r8;
    table_opcode[0xAB] = &cpu::XOR_r8;
    table_opcode[0xAC] = &cpu::XOR_r8;
    table_opcode[0xAD] = &cpu::XOR_r8;
    table_opcode[0xAE] = &cpu::XOR_r8; // XOR [HL]
    table_opcode[0xAF] = &cpu::XOR_r8; // XOR A

    // OR r8 (0xB0 - 0xB7)
    table_opcode[0xB0] = &cpu::OR_r8;  // OR B
    table_opcode[0xB1] = &cpu::OR_r8;  // OR C
    table_opcode[0xB2] = &cpu::OR_r8;
    table_opcode[0xB3] = &cpu::OR_r8;
    table_opcode[0xB4] = &cpu::OR_r8;
    table_opcode[0xB5] = &cpu::OR_r8;
    table_opcode[0xB6] = &cpu::OR_r8;  // OR [HL]
    table_opcode[0xB7] = &cpu::OR_r8;  // OR A

    // CP r8 (0xB8 - 0xBF)
    table_opcode[0xB8] = &cpu::CP_r8;
    table_opcode[0xB9] = &cpu::CP_r8;
    table_opcode[0xBA] = &cpu::CP_r8;
    table_opcode[0xBB] = &cpu::CP_r8;
    table_opcode[0xBC] = &cpu::CP_r8;
    table_opcode[0xBD] = &cpu::CP_r8;
    table_opcode[0xBE] = &cpu::CP_r8;
    table_opcode[0xBF] = &cpu::CP_r8;

    // ALU Inmediatos (d8)
    table_opcode[0xC6] = &cpu::ADD_A_d8;
    table_opcode[0xCE] = &cpu::ADC_A_d8; // ADC A, d8 (Faltaba)
    table_opcode[0xD6] = &cpu::SUB_A_d8;
    table_opcode[0xDE] = &cpu::SBC_A_d8;
    table_opcode[0xE6] = &cpu::AND_d8;   // AND d8 (Faltaba)
    table_opcode[0xEE] = &cpu::XOR_d8;
    table_opcode[0xF6] = &cpu::OR_d8;
    table_opcode[0xFE] = &cpu::CP_d8;

    // Misc ALU
    table_opcode[0x27] = &cpu::DAA;
    table_opcode[0x2F] = &cpu::CPL; // CPL (Faltaba)
    table_opcode[0x3F] = &cpu::CCF; // CCF (Faltaba)
    table_opcode[0x37] = &cpu::SCF;

    // Rotaciones Standard (No CB)
    table_opcode[0x07] = &cpu::RLCA; // RLCA (Faltaba)
    table_opcode[0x0F] = &cpu::RRCA;
    table_opcode[0x17] = &cpu::RLA;
    table_opcode[0x1F] = &cpu::RRA;

    // =============================================================
    // GRUPO: CONTROL DE FLUJO (JUMPS, CALLS, RST)
    // =============================================================

    // Jumps
    table_opcode[0xC3] = &cpu::JP;       // JP a16
    table_opcode[0xE9] = &cpu::JP_HL;    // JP HL ---> CRÍTICO PARA EL CRASH (0xE9)
    table_opcode[0x18] = &cpu::JR_d8;    // JR r8

    // Jumps Condicionales
    table_opcode[0x20] = &cpu::JR_cc_d8; // JR NZ
    table_opcode[0x28] = &cpu::JR_cc_d8; // JR Z
    table_opcode[0x30] = &cpu::JR_cc_d8; // JR NC
    table_opcode[0x38] = &cpu::JR_cc_d8; // JR C
    table_opcode[0xC2] = &cpu::JP_cc_a16; // JP NZ
    table_opcode[0xCA] = &cpu::JP_cc_a16; // JP Z
    table_opcode[0xD2] = &cpu::JP_cc_a16; // JP NC
    table_opcode[0xDA] = &cpu::JP_cc_a16; // JP C

    // Calls
    table_opcode[0xCD] = &cpu::CALL;
    // (CALL CC opcionalmente va en C4, CC, D4, DC si los implementas)

    // Returns
    table_opcode[0xC9] = &cpu::RET;
    table_opcode[0xD9] = &cpu::RETI;
    table_opcode[0xC0] = &cpu::RET_cc; // RET NZ
    table_opcode[0xC8] = &cpu::RET_cc; // RET Z
    table_opcode[0xD0] = &cpu::RET_cc; // RET NC
    table_opcode[0xD8] = &cpu::RET_cc; // RET C

    // RST (Restarts) ---> FALTABAN VARIOS
    table_opcode[0xC7] = &cpu::RST;
    table_opcode[0xCF] = &cpu::RST;
    table_opcode[0xD7] = &cpu::RST;
    table_opcode[0xDF] = &cpu::RST;
    table_opcode[0xE7] = &cpu::RST;
    table_opcode[0xEF] = &cpu::RST; // RST 28H
    table_opcode[0xF7] = &cpu::RST;
    table_opcode[0xFF] = &cpu::RST; // RST 38H

    // =============================================================
    // GRUPO: CONTROL HARDWARE Y PREFIJOS
    // =============================================================

    table_opcode[0x00] = &cpu::NOP;
    table_opcode[0x10] = &cpu::STOP;
    table_opcode[0x76] = &cpu::HALT;
    table_opcode[0xF3] = &cpu::DI;
    table_opcode[0xFB] = &cpu::EI;
    
    // Prefijo CB
    table_opcode[0xCB] = &cpu::PREFIX_CB; 
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

int cpu::executeInterrupt(int bit) {
    // DEBUG: Ver si las interrupciones se sirven
    static int irq_served_count = 0;
    irq_served_count++;
    if (irq_served_count <= 20) {
        std::cout << "[IRQ SERVICED #" << irq_served_count << "] bit=" << bit
                  << " PC_before=0x" << std::hex << PC
                  << " vector=0x" << (0x0040 + (bit * 8))
                  << " IME_was=" << (IME ? 1 : 0) << std::dec << "\n";
    }
    
    // 1. Deshabilitar IME inmediatamente es CRUCIAL
    IME = false;
    IME_scheduled = false; // Por seguridad, cancelamos cualquier EI pendiente

    // 2. Limpiar el bit en IF ANTES de saltar
    uint8_t if_before = memory.readMemory(0xFF0F);
    uint8_t if_new = if_before & ~(1 << bit);
    memory.writeMemory(0xFF0F, if_new);
    uint8_t if_after = memory.readMemory(0xFF0F);
    
    if (cpu_irq_debug_enabled) {
        std::cout << "[IRQ CLEAR] bit=" << bit 
                  << " IF_before=0x" << std::hex << (int)if_before
                  << " IF_new=0x" << (int)if_new
                  << " IF_after=0x" << (int)if_after << std::dec << "\n";
    }

    // 3. Guardar el PC actual (donde la CPU debería volver después)
    push(PC);

    // 4. Saltar al vector
    PC = 0x0040 + (bit * 8);

    // 5. Retornar los 20 T-cycles (5 M-cycles)
    return 20;
}

int cpu::handleInterrupts() {
    uint8_t if_reg = memory.readMemory(0xFF0F);
    uint8_t ie_reg = memory.readMemory(0xFFFF);

    // Interrupciones pendientes = Solicitadas (IF) AND Habilitadas (IE)
    uint8_t pending = if_reg & ie_reg & 0x1F; // Solo bits 0-4 son válidos

    if (pending > 0) {
        // DEBUG: Log wake-up from HALT
        static int wakeup_count = 0;
        if (isHalted && wakeup_count < 20) {
            wakeup_count++;
            std::cout << "[CPU WAKEUP #" << wakeup_count << "] Waking from HALT!"
                      << " IF=0x" << std::hex << (int)if_reg
                      << " IE=0x" << (int)ie_reg
                      << " PENDING=0x" << (int)pending
                      << " IME=" << std::dec << (IME ? 1 : 0)
                      << "\n";
        }
        
        if (cpu_irq_debug_enabled) {
            std::cout << "[CPU IRQ] IF=0x" << std::hex << (int)if_reg
                      << " IE=0x" << (int)ie_reg
                      << " PENDING=0x" << (int)pending
                      << " IME=" << std::dec << (IME ? 1 : 0)
                      << " HALT=" << (isHalted ? 1 : 0)
                      << " STOP=" << (isStopped ? 1 : 0)
                      << " PC=0x" << std::hex << (int)PC << std::dec << "\n";
        }

        // IMPORTANTE: Despertar a la CPU si estaba dormida
        // Esto ocurre INCLUSO si IME está deshabilitado
        if (isHalted) {
            isHalted = false;
            // HALT bug: Si IME=0 y hay interrupciones pendientes,
            // el PC no se incrementa después de HALT
            // (esto es un bug del hardware real)
            // Implementación simplificada: solo salimos de HALT
        }
        
        if (isStopped) {
            isStopped = false;
        }

        // Si el IME está activo, SERVIR la interrupción
        if (IME) {
            // Buscar la interrupción con mayor prioridad (bit más bajo)
            // Prioridad: VBlank (0) > LCD (1) > Timer (2) > Serial (3) > Joypad (4)
            for (int i = 0; i < 5; i++) {
                if ((pending >> i) & 1) {
                    if (cpu_irq_debug_enabled) {
                        std::cout << "[CPU IRQ] Servicing IRQ " << i
                                  << " vector=0x" << std::hex << (0x40 + i * 8)
                                  << std::dec << "\n";
                    }
                    return executeInterrupt(i); // Retorna los ciclos consumidos
                }
            }
        } else if (cpu_irq_debug_enabled) {
            std::cout << "[CPU IRQ] Pending but IME=0, not serviced\n";
        }
    }
    
    return 0; // No se sirvió ninguna interrupción
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
    // 1. PRIMERO: Revisar y servir interrupciones
    int interrupt_cycles = handleInterrupts();
    
    if (interrupt_cycles > 0) {
        // Se sirvió una interrupción, retornar los ciclos consumidos
        return interrupt_cycles;
    }

    // 2. Si la CPU está en HALT o STOP, solo consumir ciclos
    if (isHalted || isStopped) {
        return 4; // Consumir 4 ciclos (1 M-cycle) mientras esperamos
    }

    // 3. Fetch (Traer instrucción)
    uint16_t pc_before = PC;
    uint8_t opcode = fetch();
    
    // ============================================================
    // DEBUG: EARLY BOOT TRACE - First 100 instructions
    // ============================================================
    static int boot_trace_count = 0;
    boot_trace_count++;
    if (boot_trace_count <= 100) {
        std::cout << "[BOOT TRACE #" << std::dec << boot_trace_count 
                  << "] PC=0x" << std::hex << pc_before
                  << " OP=0x" << std::setw(2) << std::setfill('0') << (int)opcode;
        
        // Decode common opcodes for readability
        if (opcode == 0x00) std::cout << " (NOP)";
        else if (opcode == 0xC3) std::cout << " (JP a16)";
        else if (opcode == 0xAF) std::cout << " (XOR A)";
        else if (opcode == 0x21) std::cout << " (LD HL,d16)";
        else if (opcode == 0x31) std::cout << " (LD SP,d16)";
        else if (opcode == 0xE0) std::cout << " (LDH [a8],A)";
        else if (opcode == 0x3E) std::cout << " (LD A,d8)";
        else if (opcode == 0xCD) std::cout << " (CALL a16)";
        else if (opcode == 0xC9) std::cout << " (RET)";
        else if (opcode == 0xFB) std::cout << " (EI)";
        
        std::cout << std::dec << "\n";
        
        // Log key PC values we expect to see
        if (pc_before == 0x0100) std::cout << "  ^-- Entry point (should see JP)\n";
        if (pc_before == 0x0150) std::cout << "  ^-- Start: (should see JP to Init)\n";
        if (pc_before == 0x0211) std::cout << "  ^-- Init: (game initialization begins!)\n";
    }
    
    // DEBUG: Trace when game EXITS the polling loop at 0x2F0
    // JR Z at 0x2F0 doesn't jump when Z=0 (A != 0), so next PC would be 0x2F2
    static int loop_exit_count = 0;
    static uint16_t last_pc = 0;
    if (last_pc == 0x2F0 && pc_before != 0x2ED) {
        // We just executed JR Z but didn't jump back - we exited the loop!
        loop_exit_count++;
        if (loop_exit_count <= 50 || loop_exit_count % 1000 == 0) {
            std::cout << "[LOOP EXIT #" << loop_exit_count << "] Exited to PC=0x" << std::hex << pc_before
                      << " OP=0x" << (int)opcode << " A=0x" << (int)r8[A] 
                      << " FF85=" << (int)memory.HRAM[0x05] << std::dec << "\n";
        }
    }
    last_pc = pc_before;
    
    // DEBUG: Trace stuck loop at PC=0x2ED-0x2F2
    static int stuck_loop_trace = 0;
    if (pc_before >= 0x2ED && pc_before <= 0x2F2) {
        stuck_loop_trace++;
        if (stuck_loop_trace <= 20 || stuck_loop_trace % 500000 == 0) {
            std::cout << "[LOOP TRACE #" << stuck_loop_trace << "] PC=0x" << std::hex << pc_before
                      << " OP=0x" << (int)opcode
                      << " A=0x" << (int)r8[A]
                      << " Z=" << (int)getZ()
                      << " FF44=" << (int)memory.IO[0x44]
                      << " FF85=" << (int)memory.HRAM[0x05]
                      << std::dec << "\n";
        }
    }
    
    // DEBUG: Contar opcodes y detectar loops
    opcode_counts[opcode]++;
    total_instructions++;
    
    // Log periódico cada ~1 millón de instrucciones
    if (total_instructions - last_dump_instructions >= 1000000) {
        last_dump_instructions = total_instructions;
        std::cout << "\n[CPU DEBUG] After " << total_instructions << " instructions:\n";
        std::cout << "  HALT(0x76) executed: " << opcode_counts[0x76] << " times\n";
        std::cout << "  Current PC=0x" << std::hex << pc_before << std::dec << "\n";
        std::cout << "  IME=" << (IME ? 1 : 0) << " isHalted=" << (isHalted ? 1 : 0) << "\n";
        std::cout << "  IF=0x" << std::hex << (int)memory.IO[0x0F]
                  << " IE=0x" << (int)memory.readMemory(0xFFFF) << std::dec << "\n";
        
        // Mostrar los 5 opcodes más ejecutados
        int top5[5] = {0, 0, 0, 0, 0};
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 5; j++) {
                if (opcode_counts[i] > opcode_counts[top5[j]]) {
                    for (int k = 4; k > j; k--) top5[k] = top5[k-1];
                    top5[j] = i;
                    break;
                }
            }
        }
        std::cout << "  Top opcodes: ";
        for (int j = 0; j < 5; j++) {
            std::cout << "0x" << std::hex << top5[j] << "(" << std::dec << opcode_counts[top5[j]] << ") ";
        }
        std::cout << "\n";
    }

    // DEBUG: Imprimir estado de CPU antes de ejecutar
    uint8_t if_reg = memory.readMemory(0xFF0F);
    uint8_t ie_reg = memory.readMemory(0xFFFF);
    // std::cout << "[CPU] PC=0x" << std::hex << pc_before 
    //           << " OP=0x" << (int)opcode
    //           << " IME=" << std::dec << (IME ? 1 : 0)
    //           << " IME_sched=" << (IME_scheduled ? 1 : 0)
    //           << " IF=0x" << std::hex << (int)if_reg
    //           << " IE=0x" << (int)ie_reg
    //           << " A=" << (int)r8[A] << " F=" << (int)r8[F]
    //           << " SP=0x" << SP << std::dec << "\n";

    // 4. Decode & Execute
    int cycles = 4;
    
    if (table_opcode[opcode] != nullptr)
    {
        cycles = (this->*table_opcode[opcode])(opcode);
    } 
    else
    {
        std::cout << "Opcode no implementado: 0x" << std::hex << (int)opcode 
                  << " at PC=0x" << (PC-1) << "\n";
        cycles = 4; // Retornar al menos 4 ciclos para evitar loops infinitos
    }
    
    // 5. DESPUÉS de ejecutar la instrucción: Activar IME si estaba programado
    // EI habilita interrupciones DESPUÉS de la siguiente instrucción
    if (IME_scheduled) {
        IME = true;
        IME_scheduled = false;
        std::cout << "[CPU] IME activado tras instrucción en PC=0x" << std::hex << pc_before << std::dec << "\n";
    }
    
    return cycles;
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
    //std::cout <<"DAA " << "\n";

    // NOTA: No hacemos PC++ aquí, fetch() ya lo hizo.
    return 4; 
}

int cpu::STOP(uint8_t opcode)
{
    (void)opcode;
    // STOP es 0x10 0x00, saltamos el byte extra
    PC++; 
    isStopped = true;
    //std::cout <<"stop " << "\n";
    return 4;
}

int cpu::HALT(uint8_t opcode)
{
    (void)opcode;
    
    // DEBUG: Log CADA ejecución de HALT
    std::cout << "[CPU] *** HALT EXECUTED *** PC=0x" << std::hex << PC
              << " IME=" << std::dec << (IME ? 1 : 0)
              << " IF=0x" << std::hex << (int)memory.IO[0x0F]
              << " IE=0x" << (int)memory.readMemory(0xFFFF)
              << std::dec << "\n";
    
    isHalted = true;
    
    // HALT bug del hardware real:
    // Si IME=0 y hay interrupciones pendientes,
    // el PC no se incrementa después de HALT
    uint8_t if_reg = memory.readMemory(0xFF0F);
    uint8_t ie_reg = memory.readMemory(0xFFFF);
    uint8_t pending = if_reg & ie_reg & 0x1F;
    
    if (!IME && pending > 0) {
        // HALT bug: salir inmediatamente de HALT
        isHalted = false;
        std::cout << "[CPU] HALT BUG triggered - exiting immediately\n";
    }
    
    return 4;
}

int cpu::NOP(uint8_t opcode) {
    (void)opcode;
    //std::cout <<"nop " << "\n";
    return 4;
}

int cpu::JP(uint8_t opcode) {
    (void)opcode;
    uint16_t targetAddress = readImmediateWord(); 
    PC = targetAddress;
    //std::cout <<"jump " << "\n";
    return 16;
}

int cpu::DI(uint8_t opcode)
{
    (void)opcode;
    IME = false;
    IME_scheduled = false; // Cancelar cualquier EI pendiente
    return 4;
}

int cpu::EI(uint8_t opcode)
{
    (void)opcode;
    
    // IMPORTANTE: EI NO habilita interrupciones inmediatamente
    // Se habilitan DESPUÉS de la siguiente instrucción
    IME_scheduled = true;
    
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
            //std::cout <<"LD [HL], r8 " << "\n";

        return 8; // Escribir en memoria toma más tiempo
    } 
    else if (srcBits == 6) { // LD r8, [HL]
        uint16_t address = getHL();
        uint8_t value = memory.readMemory(address);
        r8[hardwareToYourEnum[destBits]] = value;
         //std::cout <<"LD r8, [HL] " << "\n";
        return 8; // Leer de memoria toma más tiempo
    } 
    else { // LD r8, r8 (Registro a Registro)
         //std::cout <<"LD r8, r8 " << "\n";
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

    //std::cout << "XOR A\n"; 
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

    //std::cout << "LD r16, d16 val=" << std::hex << value << "\n";
    return 12; // Ciclos
}
int cpu::LDD_HL_A(uint8_t opcode) {
    (void)opcode;
    
    // 1. Escribir A en la dirección (HL)
    uint16_t addr = getHL();
    memory.writeMemory(addr, r8[A]);

    // 2. Decrementar HL
    setHL(addr - 1);

    //std::cout << "LD (HL-), A\n";
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
        //std::cout << "LD [HL], " << (int)value << "\n";
        return 12; // Escribir en memoria es más lento
    } else {
        r8[hardwareToYourEnum[regBits]] = value;
        //std::cout << "LD " << (int)regBits << ", " << (int)value << "\n";
        return 8; 
    }
}
int cpu::DEC_r8(uint8_t opcode) {
    int regBits = (opcode >> 3) & 0x07;
    static const R8 hardwareToYourEnum[] = {B, C, D, E, H, L, H, A}; 
    R8 target = hardwareToYourEnum[regBits];

    // Handle [HL] case specially
    if (regBits == 6) {
        uint16_t addr = getHL();
        uint8_t prevValue = memory.readMemory(addr);
        uint8_t newValue = prevValue - 1;
        memory.writeMemory(addr, newValue);
        
        setZ(newValue == 0);
        setN(true);
        setH((prevValue & 0x0F) == 0);
        return 12;
    }

    uint8_t prevValue = r8[target];
    r8[target]--;

    // DEBUG: Track DEC B during Init loop (PC=0x215)
    static int dec_b_trace = 0;
    if (opcode == 0x05) {  // DEC B
        dec_b_trace++;
        if (dec_b_trace <= 10 || (dec_b_trace % 256 == 0 && dec_b_trace <= 5000)) {
            std::cout << "[DEC B #" << dec_b_trace << "] Before=" << std::hex 
                      << (int)prevValue << " After=" << (int)r8[target]
                      << " Z=" << std::dec << (r8[target] == 0 ? 1 : 0) << "\n";
        }
        // Special trace when B becomes 0
        if (r8[target] == 0 && dec_b_trace <= 5000) {
            std::cout << "[DEC B] B reached 0! Should exit inner loop. Z will be set to 1\n";
        }
    }

    // Banderas
    setZ(r8[target] == 0);
    setN(true); // Siempre true en decrementos
    // Half Carry: se activa si hubo préstamo del bit 4 (0x0F -> 0x10)
    setH((prevValue & 0x0F) == 0); 
    
    return 4;
}
int cpu::JR_cc_r8(uint8_t opcode) {
    // PC BEFORE reading operand (gives us the instruction address)
    uint16_t instr_pc = PC - 1;  // -1 because opcode was already fetched
    
    // Leer el desplazamiento (signed!)
    int8_t offset = (int8_t)readImmediateByte(); 
    uint16_t target_pc = PC + offset;
    
    // Determinar la condición
    int condition = (opcode >> 3) & 0x03;
    bool jump = false;

    switch(condition) {
        case 0: jump = (getZ() == 0); break; // NZ
        case 1: jump = (getZ() == 1); break; // Z
        case 2: jump = (getC() == 0); break; // NC
        case 3: jump = (getC() == 1); break; // C
    }

    // DEBUG: Trace JR NZ at Init loop (PC=0x216)
    static int jr_init_trace = 0;
    if (instr_pc == 0x216 && opcode == 0x20) {  // JR NZ in Init loop
        jr_init_trace++;
        if (jr_init_trace <= 10 || (jr_init_trace % 256 == 0 && jr_init_trace <= 5000)) {
            std::cout << "[JR NZ @0x216 #" << jr_init_trace << "] Z=" << (int)getZ()
                      << " Jump=" << (jump ? "YES" : "NO")
                      << " B=" << std::hex << (int)r8[B] 
                      << " C=" << (int)r8[C] << std::dec << "\n";
        }
        // Trace when we should NOT jump (Z=1)
        if (getZ() == 1) {
            std::cout << "[JR NZ @0x216] Z=1, should NOT jump! Continuing to 0x218\n";
        }
    }

    // DEBUG: Trace jump decisions in critical loop (0x2ED-0x2F2)
    static int jr_trace_count = 0;
    if ((PC - 2) >= 0x2ED && (PC - 2) <= 0x2F2) {  // PC already advanced by 2 (opcode + offset)
        jr_trace_count++;
        if (jr_trace_count <= 50) {
            const char* cond_names[] = {"NZ", "Z", "NC", "C"};
            std::cout << "[JR TRACE #" << jr_trace_count << "] Cond=" << cond_names[condition]
                      << " Z=" << (int)getZ() << " Jump=" << (jump ? "YES" : "NO")
                      << " Offset=" << (int)offset
                      << " Target=0x" << std::hex << target_pc << std::dec << "\n";
        }
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
    
    // DEBUG: Track writes to 0xFF85 with PC context
    if (offset == 0x85) {
        static int ff85_ldh_write_count = 0;
        ff85_ldh_write_count++;
        if (ff85_ldh_write_count <= 30) {
            // PC-2 because we already fetched the opcode (E0) and operand (85)
            uint16_t instr_pc = PC - 2;
            std::cout << "[FF85 WRITE #" << ff85_ldh_write_count << "] PC=0x" 
                      << std::hex << instr_pc << " A=0x" << (int)r8[A]
                      << " (LDH [0xFF85],A)" << std::dec << "\n";
        }
    }
    
    // Escribe A en 0xFF00 + offset
    memory.writeMemory(0xFF00 | offset, r8[A]);
    
    //std::cout << "LDH [FF" << std::hex << (int)offset << "], A\n";
    return 12;
}

int cpu::LDH_A_n(uint8_t opcode) {
    (void)opcode;
    uint8_t offset = readImmediateByte();
    
    // DEBUG: Rastrear qué registros lee el juego en su polling loop
    static int ldh_counts[256] = {0};
    static int total_ldh = 0;
    ldh_counts[offset]++;
    total_ldh++;
    
    if (total_ldh == 10000) {
        std::cout << "[LDH DEBUG] After 10K LDH reads:\n";
        for (int i = 0; i < 256; i++) {
            if (ldh_counts[i] > 100) {
                std::cout << "  0xFF" << std::hex << std::setw(2) << std::setfill('0') << i 
                          << ": " << std::dec << ldh_counts[i] << " reads\n";
            }
        }
    }
    
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
    
    // DEBUG: Track writes to 0xFF85 with PC context
    if (addr == 0xFF85) {
        static int ff85_ld16_write_count = 0;
        ff85_ld16_write_count++;
        if (ff85_ld16_write_count <= 30) {
            // PC-3 because we already fetched opcode (EA) and 2-byte operand
            uint16_t instr_pc = PC - 3;
            std::cout << "[FF85 WRITE via LD #" << ff85_ld16_write_count << "] PC=0x" 
                      << std::hex << instr_pc << " A=0x" << (int)r8[A]
                      << " (LD [0xFF85],A)" << std::dec << "\n";
        }
    }
    
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
    // 1. LEER el verdadero opcode CB
    uint8_t cb_op = readImmediateByte();
    
    // Decodificar
    int regIndex = cb_op & 0x07;       // 0-7: Registro afectado
    int op = (cb_op >> 3) & 0x07;      // 0-7: Bit (o sub-operación en rotaciones)
    int type = (cb_op >> 6) & 0x03;    // 0-3: Grupo (Rot, Bit, Res, Set)

    static const R8 map[] = {B, C, D, E, H, L, H, A}; 
    uint8_t val;
    int cycles;

    // 2. LEER VALOR
    if (regIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        // Ciclos base: BIT=12, Rot/Set/Res=16
        cycles = (type == 1) ? 12 : 16; 
    } else {
        val = r8[map[regIndex]];
        // Ciclos base: BIT=8, Rot/Set/Res=8
        cycles = 8;
    }

    // 3. EJECUTAR OPERACIÓN
    switch (type) {
        // --- TYPE 0: ROTACIONES Y SHIFTS (0x00 - 0x3F) ---
        // Aquí 'op' decide qué rotación es (RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL)
        case 0: {
            bool C_flag = getC(); // Carry actual
            bool newCarry = false;
            
            switch (op) {
                case 0: // RLC (Rotate Left Circular) - Bit 7 va al Carry y al Bit 0
                    newCarry = (val & 0x80) != 0;
                    val = (val << 1) | (newCarry ? 1 : 0);
                    break;
                case 1: // RRC (Rotate Right Circular) - Bit 0 va al Carry y al Bit 7
                    newCarry = (val & 0x01) != 0;
                    val = (val >> 1) | (newCarry ? 0x80 : 0);
                    break;
                case 2: // RL (Rotate Left thru Carry) - Carry viejo al Bit 0
                    newCarry = (val & 0x80) != 0;
                    val = (val << 1) | (C_flag ? 1 : 0);
                    break;
                case 3: // RR (Rotate Right thru Carry) - Carry viejo al Bit 7
                    newCarry = (val & 0x01) != 0;
                    val = (val >> 1) | (C_flag ? 0x80 : 0);
                    break;
                case 4: // SLA (Shift Left Arithmetic) - Bit 0 se vuelve 0
                    newCarry = (val & 0x80) != 0;
                    val = val << 1;
                    break;
                case 5: // SRA (Shift Right Arithmetic) - Bit 7 se mantiene (Signo)
                    newCarry = (val & 0x01) != 0;
                    val = (val >> 1) | (val & 0x80);
                    break;
                case 6: // SWAP (Intercambiar Nibbles)
                    {
                        uint8_t low = val & 0x0F;
                        uint8_t high = val & 0xF0;
                        val = (low << 4) | (high >> 4);
                        newCarry = false; // SWAP limpia Carry
                    }
                    break;
                case 7: // SRL (Shift Right Logical) - Bit 7 se vuelve 0
                    newCarry = (val & 0x01) != 0;
                    val = val >> 1;
                    break;
            }

            // Banderas comunes para Rotaciones/Shifts CB
            setZ(val == 0);
            setN(false);
            setH(false);
            setC(newCarry);
            break; 
        }

        // --- TYPE 1: BIT TEST (0x40 - 0x7F) ---
        // (Tu implementación estaba bien, solo la integro aquí)
        case 1: {
            bool isZero = (val & (1 << op)) == 0;
            setZ(isZero);
            setN(false);
            setH(true);
            // C no cambia
            return cycles; // RETORNAMOS AQUÍ (BIT no escribe en memoria)
        }

        // --- TYPE 2: RES (Reset Bit) (0x80 - 0xBF) ---
        case 2:
            val &= ~(1 << op);
            break;

        // --- TYPE 3: SET (Set Bit) (0xC0 - 0xFF) ---
        case 3:
            val |= (1 << op);
            break;
    }

    // 4. ESCRIBIR RESULTADO (Write Back)
    // Esto faltaba en tu código. BIT no llega aquí, pero el resto sí.
    if (regIndex == 6) { // [HL]
        memory.writeMemory(getHL(), val);
    } else {
        r8[map[regIndex]] = val;
    }

    return cycles;
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
    
    // DEBUG: Track writes to 0xFF85 via LD (C),A
    if (addr == 0xFF85) {
        static int ff85_ldc_write_count = 0;
        ff85_ldc_write_count++;
        if (ff85_ldc_write_count <= 30) {
            // PC-1 because we already fetched opcode (E2)
            uint16_t instr_pc = PC - 1;
            std::cout << "[FF85 WRITE via LD(C) #" << ff85_ldc_write_count << "] PC=0x" 
                      << std::hex << instr_pc << " A=0x" << (int)r8[A]
                      << " C=0x" << (int)r8[C] << std::dec << "\n";
        }
    }
    
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
    
    // 1. Pop PC del stack (como RET normal)
    PC = pop();
    
    // 2. Habilitar interrupciones INMEDIATAMENTE (sin delay)
    IME = true;
    IME_scheduled = false;
    
    return 16; // 16 ciclos
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
// Opcode 0x2F: CPL (Complemento a uno de A)
// Invierte todos los bits de A (A = ~A)
int cpu::CPL(uint8_t opcode) {
    (void)opcode;
    r8[A] = ~r8[A];
    
    setN(true);  // N siempre 1
    setH(true);  // H siempre 1
    
    return 4; // 4 ciclos
}

// Opcode 0xE6: AND d8 (AND inmediato)
// A = A & valor_inmediato
int cpu::AND_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte(); // Lee el dato d8
    
    r8[A] &= val;
    
    setZ(r8[A] == 0);
    setN(false);
    setH(true);  // ¡OJO! AND siempre pone Half Carry a 1 en GB
    setC(false);
    
    return 8; // 8 ciclos
}
// --- IMPLEMENTACIÓN XOR (0xA8 - 0xAF) ---
int cpu::XOR_r8(uint8_t opcode) {
    int srcIndex = opcode & 0x07; // Bits 0-2 indican la fuente
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    if (srcIndex == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 8;
    } else {
        val = r8[map[srcIndex]];
    }

    // Operación XOR: A = A ^ val
    r8[A] ^= val;

    // Flags: Z si es cero, todo lo demás en false
    setZ(r8[A] == 0);
    setN(false);
    setH(false);
    setC(false);

    return cycles;
}

// --- IMPLEMENTACIÓN ADC Inmediato (0xCE) ---
int cpu::ADC_A_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte(); // Leemos el dato d8
    
    int carry = getC() ? 1 : 0;
    int result = r8[A] + val + carry;

    // Flags
    setZ((result & 0xFF) == 0);
    setN(false);
    // Half Carry: (Bits bajos de A) + (Bits bajos de Val) + Carry > 15
    setH(((r8[A] & 0x0F) + (val & 0x0F) + carry) > 0x0F);
    // Carry: Resultado mayor a 255
    setC(result > 0xFF);

    r8[A] = (uint8_t)result;
    return 8;
}
// Rotate Left Circular Accumulator
// Rota A hacia la izquierda. El bit 7 pasa al Carry Y al bit 0.
int cpu::RLCA(uint8_t opcode) {
    (void)opcode;
    uint8_t val = r8[A];
    
    // Obtenemos el bit más significativo (Bit 7)
    bool carry = (val & 0x80) != 0;

    // Rotamos: Desplazamos a la izquierda 1 y metemos el carry en el bit 0
    r8[A] = (val << 1) | (carry ? 1 : 0);

    // --- BANDERAS ---
    setZ(false); // IMPORTANTE: RLCA siempre pone Zero en false en la GB
    setN(false);
    setH(false);
    setC(carry); // El Carry toma el valor del bit que "salió" (el antiguo bit 7)

    return 4; // 4 ciclos
}
// Restart
// Funciona como un CALL a una dirección fija (0x00, 0x08, 0x10, ..., 0x38)
int cpu::RST(uint8_t opcode) {
    // La dirección destino está codificada en los bits 3, 4 y 5 del opcode.
    // Ejemplo: 0xEF (1110 1111) AND 0x38 (0011 1000) = 0x28
    uint16_t target = opcode & 0x38;

    // 1. Hacemos PUSH del PC actual al Stack
    // (Guardamos la dirección de retorno)
    push(PC); 

    // 2. Saltamos a la dirección destino
    PC = target;

    return 16; // 16 ciclos
}
int cpu::JP_HL(uint8_t opcode) {
    (void)opcode;
    
    // Simplemente cargamos el valor de HL en el Program Counter
    PC = getHL();

    return 4; // 4 ciclos
}
// Opcode 0x17: RLA (Rotate Left Accumulator through Carry)
// Rota el registro A a la izquierda A TRAVÉS del Carry.
// [C] <- [7][6][5][4][3][2][1][0] <- [C antiguo]
int cpu::RLA(uint8_t opcode) {
    (void)opcode;
    uint8_t val = r8[A];
    
    // 1. Capturamos el Carry actual para meterlo en el bit 0
    bool oldCarry = getC();
    
    // 2. Capturamos el bit 7 actual para que sea el nuevo Carry
    bool newCarry = (val & 0x80) != 0;

    // 3. Rotación: Desplazar izquierda + meter Carry viejo en bit 0
    r8[A] = (val << 1) | (oldCarry ? 1 : 0);

    // 4. Banderas
    setZ(false); // ¡CRÍTICO! En RLA (0x17) Z siempre es 0.
    setN(false);
    setH(false);
    setC(newCarry);

    return 4; // 4 ciclos
}

// Opcode 0x1F: RRA (Rotate Right Accumulator through Carry)
// Rota el registro A a la derecha A TRAVÉS del Carry.
// [C antiguo] -> [7][6][5][4][3][2][1][0] -> [C]
int cpu::RRA(uint8_t opcode) {
    (void)opcode;
    uint8_t val = r8[A];

    // 1. Capturamos el Carry actual para meterlo en el bit 7
    bool oldCarry = getC();

    // 2. Capturamos el bit 0 actual para que sea el nuevo Carry
    bool newCarry = (val & 0x01) != 0;

    // 3. Rotación: Desplazar derecha + meter Carry viejo en bit 7
    r8[A] = (val >> 1) | (oldCarry ? 0x80 : 0);

    // 4. Banderas
    setZ(false); // ¡CRÍTICO! En RRA (0x1F) Z siempre es 0.
    setN(false);
    setH(false);
    setC(newCarry);

    return 4; // 4 ciclos
}
// Opcode 0x3F: CCF (Complement Carry Flag)
// Invierte el valor de la bandera Carry (C = !C)
int cpu::CCF(uint8_t opcode) {
    (void)opcode;
    
    // Banderas afectadas:
    setN(false);   // Reset N
    setH(false);   // Reset H
    setC(!getC()); // Invertir C
    
    // Z no se ve afectada
    
    return 4; // 4 ciclos
}

// Opcode 0xEE: XOR d8 (Exclusive OR Immediate)
// A = A ^ valor_inmediato
int cpu::XOR_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte(); // Leer el dato d8
    
    r8[A] ^= val;
    
    // Banderas:
    setZ(r8[A] == 0);
    setN(false);
    setH(false);
    setC(false);
    
    return 8; // 8 ciclos
}

// Opcode 0xF6: OR d8 (Logical OR Immediate)
// A = A | valor_inmediato
int cpu::OR_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte(); // Leer el dato d8
    
    r8[A] |= val;
    
    // Banderas:
    setZ(r8[A] == 0);
    setN(false);
    setH(false);
    setC(false);
    
    return 8; // 8 ciclos
}
// Opcode 0xB8 - 0xBF: CP r8 (Compare A with r8)
// Flags: Z (si iguales), N=1, H (si borrow en bit 4), C (si A < val)
int cpu::CP_r8(uint8_t opcode) {
    int index = opcode & 0x07; // Bits 0-2 indican el registro fuente
    static const R8 map[] = {B, C, D, E, H, L, H, A};
    
    uint8_t val;
    int cycles = 4;

    // 1. Obtener valor a comparar
    if (index == 6) { // [HL]
        val = memory.readMemory(getHL());
        cycles = 8;
    } else {
        val = r8[map[index]];
    }

    uint8_t a = r8[A];

    // 2. Comparación (Lógica de resta pero sin guardar resultado)
    setZ(a == val);
    setN(true); // CP siempre es una "resta"
    
    // Half Carry: Si hay préstamo del bit 4
    setH((a & 0x0F) < (val & 0x0F));
    
    // Carry: Si A es menor que val (Préstamo total)
    setC(a < val);

    return cycles;
}
// Opcode 0x0A: LD A, [BC]
// Carga en el registro A el valor almacenado en la dirección de memoria apuntada por BC.
int cpu::LD_A_BC(uint8_t opcode) {
    (void)opcode;

    // 1. Obtener la dirección de memoria desde el par de registros BC
    uint16_t addr = getBC();

    // 2. Leer el valor de esa dirección y guardarlo en A
    r8[A] = memory.readMemory(addr);

    return 8; // 8 ciclos
}
// Opcode 0xD6: SUB A, d8 (Subtract Immediate)
// A = A - d8
int cpu::SUB_A_d8(uint8_t opcode) {
    (void)opcode;
    uint8_t val = readImmediateByte(); // Leer d8
    uint8_t a = r8[A];

    int result = a - val;

    // Banderas
    setZ((result & 0xFF) == 0);
    setN(true); // Resta
    setH((a & 0x0F) < (val & 0x0F)); // Half Carry (Borrow)
    setC(a < val); // Carry (Borrow)

    // Guardar resultado
    r8[A] = (uint8_t)result;

    return 8; // 8 ciclos
}
// Opcode 0x3A: LDD A, [HL] (Load A from HL and Dec HL)
// También conocido como LD A, [HL-]
int cpu::LDD_A_HL(uint8_t opcode) {
    (void)opcode;
    
    uint16_t hl = getHL();
    
    // 1. Cargar valor
    r8[A] = memory.readMemory(hl);
    
    // 2. Decrementar HL
    setHL(hl - 1);

    return 8; // 8 ciclos
}
// Opcode 0x02: LD [BC], A
// Guarda el contenido de A en la dirección de memoria apuntada por BC.
int cpu::LD_BC_A(uint8_t opcode) {
    (void)opcode;
    
    // 1. Obtener la dirección destino desde BC
    uint16_t addr = getBC();
    
    // 2. Escribir el valor de A en esa dirección
    memory.writeMemory(addr, r8[A]);
    
    return 8; // 8 ciclos
}
// Opcode 0xF2: LD A, [C]
// Lee de la dirección 0xFF00 + registro C
int cpu::LD_A_C(uint8_t opcode) {
    (void)opcode;
    // La dirección es High RAM (0xFF00) + el valor de C
    uint16_t addr = 0xFF00 | r8[C]; 
    
    r8[A] = memory.readMemory(addr);
    
    return 8; // 8 ciclos
}