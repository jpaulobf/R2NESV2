#include "Core/Cartridge/Mappers/Mapper000.h"

namespace R2NES::Core
{
    Mapper000::Mapper000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

    Mapper000::~Mapper000() {}

    bool Mapper000::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        // PRG ROM no Mapper 000 fica entre 0x8000 e 0xFFFF
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Se tivermos 16KB de PRG ROM, os endereços 0xC000-0xFFFF são espelhos de 0x8000-0xBFFF
            mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool Mapper000::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        // Mapper 000 não suporta escrita na PRG ROM. Todas as escritas em $8000-$FFFF são ignoradas.
        // Retornamos false para não tratar a escrita.
        // if (addr >= 0x8000 && addr <= 0xFFFF)
        // {
        //     mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
        //     return true;
        // }
        return false;
    }

    bool Mapper000::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        // CHR ROM no Mapper 000 fica entre 0x0000 e 0x1FFF
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool Mapper000::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }
}