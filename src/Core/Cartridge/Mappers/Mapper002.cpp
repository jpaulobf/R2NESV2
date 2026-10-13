#include "Core/Cartridge/Mappers/Mapper002.h"
#include <stdexcept>

namespace R2NES::Core
{
    Mapper002::Mapper002(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
    {
        if (prgBanks == 0 || prgBanks > 256 || (prgBanks & (prgBanks - 1)) != 0 ||
            chrBanks > 1 || (mirror != MirrorMode::HORIZONTAL && mirror != MirrorMode::VERTICAL))
            throw std::invalid_argument("Unsupported Mapper 2 memory configuration");
        mirrorMode = mirror;
        reset();
    }

    Mapper002::~Mapper002() {}

    bool Mapper002::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xBFFF)
        {
            // Banco selecionável de 16KB
            mapped_addr = nPRGBankSelect * 0x4000 + (addr & 0x3FFF);
            return true;
        }

        if (addr >= 0xC000 && addr <= 0xFFFF)
        {
            // Banco fixo: Último banco de 16KB da PRG ROM
            mapped_addr = (nPRGBanks - 1) * 0x4000 + (addr & 0x3FFF);
            return true;
        }

        return false;
    }

    bool Mapper002::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Escrita em qualquer lugar de $8000-$FFFF altera o banco de $8000-$BFFF
            nPRGBankSelect = data & (nPRGBanks - 1);
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        return false;
    }

    bool Mapper002::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool Mapper002::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF && nCHRBanks == 0)
        {
            // Somente CHR-RAM aceita escritas; CHR-ROM permanece imutável.
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    MirrorMode Mapper002::getMirrorMode()
    {
        // Retorna o modo definido no hardware (passado via constructor)
        return mirrorMode;
    }

    void Mapper002::reset()
    {
        nPRGBankSelect = 0;
    }

    void Mapper002::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        // Mantém os cinco bytes dos estados anteriores no Windows (enum de 32 bits LE).
        const uint32_t mirror = static_cast<uint32_t>(mirrorMode);
        for (unsigned shift = 0; shift < 32; shift += 8)
            os.put(static_cast<char>((mirror >> shift) & 0xFF));
    }

    void Mapper002::loadState(std::istream &is)
    {
        uint8_t bank = 0;
        uint32_t mirror = 0;
        is.read(reinterpret_cast<char *>(&bank), sizeof(bank));
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            const int byte = is.get();
            if (byte == std::char_traits<char>::eof())
                return;
            mirror |= static_cast<uint32_t>(static_cast<uint8_t>(byte)) << shift;
        }
        if (!is || bank >= nPRGBanks || mirror != static_cast<uint32_t>(mirrorMode))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        nPRGBankSelect = bank;
    }
}
