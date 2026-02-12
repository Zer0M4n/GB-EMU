#include "MBC3.h"
#include <iostream>

// ============================================================
//  Constructor
// ============================================================

MBC3::MBC3(std::vector<uint8_t>& rom_ref,
           std::vector<uint8_t>& ram_ref,
           uint16_t              banks)
    : rom(rom_ref)
    , ram(ram_ref)
    , totalRomBanks(banks)
    , romBank(1)
    , ramRtcSelect(0)
    , ramEnabled(false)
    , latchValue(0xFF)
{
    for (int i = 0; i < 5; ++i)
    {
        rtcRegisters[i]        = 0;
        latchedRtcRegisters[i] = 0;
    }

    std::cout << "[MBC3] Inicializado. ROM banks: " << std::dec
              << totalRomBanks << " | RAM: " << ram.size() << " bytes\n";
}

// ============================================================
//  Lectura ROM
// ============================================================

uint8_t MBC3::readROM(uint16_t address)
{
    // Banco 0 fijo: 0x0000 - 0x3FFF
    if (address <= 0x3FFF)
    {
        return rom[address];
    }

    // Banco conmutable: 0x4000 - 0x7FFF
    if (address >= 0x4000 && address <= 0x7FFF)
    {
        // Máscara correcta: necesita manejar cuando totalRomBanks es potencia de 2
        // Para Pokémon Rojo: 128 bancos → máscara 0x7F
        uint8_t  bank   = romBank & (totalRomBanks - 1);
        if (bank == 0) bank = 1; // MBC3 nunca mapea banco 0 en área conmutable

        uint32_t offset = static_cast<uint32_t>(address - 0x4000)
                        + static_cast<uint32_t>(bank) * 0x4000u;

        if (offset >= rom.size())
        {
            std::cerr << "[MBC3] readROM OOB! offset=0x" << std::hex << offset
                      << " bank=" << (int)bank
                      << " rom.size()=0x" << rom.size() << std::dec << "\n";
            return 0xFF;
        }
        return rom[offset];
    }

    return 0xFF;
}

// ============================================================
//  Escritura ROM (registros de control del MBC)
// ============================================================

void MBC3::writeROM(uint16_t address, uint8_t value)
{
    // 0x0000 - 0x1FFF : Habilitar/deshabilitar RAM y RTC
    if (address <= 0x1FFF)
    {
        ramEnabled = ((value & 0x0F) == 0x0A);
        return;
    }

    // 0x2000 - 0x3FFF : Selección de banco ROM (7 bits)
    if (address <= 0x3FFF)
    {
        romBank = value & 0x7F;
        if (romBank == 0) romBank = 1; // El banco 0 no se puede seleccionar aquí
        return;
    }

    // 0x4000 - 0x5FFF : Selección de banco RAM (0x00-0x03) o registro RTC (0x08-0x0C)
    if (address <= 0x5FFF)
    {
        ramRtcSelect = value;
        return;
    }

    // 0x6000 - 0x7FFF : Latch del RTC (escribe 0x00 luego 0x01 para congelar)
    if (address <= 0x7FFF)
    {
        if (latchValue == 0x00 && value == 0x01)
        {
            // Congelar (latch) el RTC: actualizar los valores del sistema
            // y copiarlos a los registros que el juego puede leer
            updateRTC();
            for (int i = 0; i < 5; ++i)
                latchedRtcRegisters[i] = rtcRegisters[i];
        }
        latchValue = value;
        return;
    }
}

// ============================================================
//  Lectura RAM / RTC
// ============================================================

uint8_t MBC3::readRAM(uint16_t address)
{
    if (!ramEnabled) return 0xFF;

    // Bancos de RAM: 0x00 - 0x03
    if (ramRtcSelect <= 0x03)
    {
        if (ram.empty()) return 0xFF;

        uint32_t offset = static_cast<uint32_t>(address - 0xA000)
                        + static_cast<uint32_t>(ramRtcSelect) * 0x2000u;

        if (offset >= ram.size())
        {
            std::cerr << "[MBC3] readRAM OOB! offset=0x" << std::hex << offset
                      << " ramBank=" << (int)ramRtcSelect
                      << " ram.size()=0x" << ram.size() << std::dec << "\n";
            return 0xFF;
        }
        return ram[offset];
    }

    // Registros RTC latched: 0x08 (segundos) → 0x0C (DH)
    if (ramRtcSelect >= 0x08 && ramRtcSelect <= 0x0C)
    {
        return latchedRtcRegisters[ramRtcSelect - 0x08];
    }

    return 0xFF;
}

// ============================================================
//  Escritura RAM / RTC
// ============================================================

void MBC3::writeRAM(uint16_t address, uint8_t value)
{
    if (!ramEnabled) return;

    // Bancos de RAM: 0x00 - 0x03
    if (ramRtcSelect <= 0x03)
    {
        if (ram.empty()) return;

        uint32_t offset = static_cast<uint32_t>(address - 0xA000)
                        + static_cast<uint32_t>(ramRtcSelect) * 0x2000u;

        if (offset >= ram.size())
        {
            std::cerr << "[MBC3] writeRAM OOB! offset=0x" << std::hex << offset
                      << std::dec << "\n";
            return;
        }
        ram[offset] = value;
        return;
    }

    // Registros RTC: 0x08 - 0x0C (escritura directa al RTC interno)
    if (ramRtcSelect >= 0x08 && ramRtcSelect <= 0x0C)
    {
        rtcRegisters[ramRtcSelect - 0x08] = value;
        return;
    }
}

// ============================================================
//  Actualizar RTC desde el reloj del sistema
// ============================================================

void MBC3::updateRTC()
{
    time_t    t   = time(nullptr);
    struct tm* now = localtime(&t);

    // Registros 0-2: segundos, minutos, horas (rangos oficiales del hardware)
    rtcRegisters[0] = static_cast<uint8_t>(now->tm_sec  & 0x3F); // 0-59
    rtcRegisters[1] = static_cast<uint8_t>(now->tm_min  & 0x3F); // 0-59
    rtcRegisters[2] = static_cast<uint8_t>(now->tm_hour & 0x1F); // 0-23

    // Registros 3-4: contador de días (9 bits total)
    // tm_yday va de 0 a 365 → suficiente para DL (8 bits) + DH bit 0 (1 bit)
    uint16_t days   = static_cast<uint16_t>(now->tm_yday);
    rtcRegisters[3] = static_cast<uint8_t>(days & 0xFF);           // DL: bits 0-7
    rtcRegisters[4] = static_cast<uint8_t>((days >> 8) & 0x01);    // DH: solo bit 0
    // Nota: DH también tiene bit 6 (halt) y bit 7 (day counter carry).
    // Preservamos los bits que el juego puede haber escrito previamente:
    rtcRegisters[4] |= (rtcRegisters[4] & 0xFE); // conserva halt/carry si se setearon
}