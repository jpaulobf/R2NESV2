#include "Core/Cartridge/Mappers/Mapper002.h"

namespace R2NES::Core
{
    Mapper002::Mapper002(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
    {
        nPRGBankSelect = 0;
        mirrorMode = mirror;
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
            nPRGBankSelect = data & 0x0F; // Mapper 2 usa os 4 bits baixos para seleção
        }

        // Retornamos false porque o Mapper 2 não tem PRG-RAM ou escritas na ROM
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
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            // Mapper 2 (UNROM) sempre utiliza CHR-RAM de 8KB.
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

    void Mapper002::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
    }

    void Mapper002::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
    }
}
