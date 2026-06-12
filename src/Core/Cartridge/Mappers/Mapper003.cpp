#include "Core/Cartridge/Mappers/Mapper003.h"

namespace R2NES::Core
{
    Mapper003::Mapper003(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
    {
        nCHRBankSelect = 0;
        mirrorMode = mirror;
    }

    Mapper003::~Mapper003() {}

    bool Mapper003::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // PRG ROM: 16KB (espelhado) ou 32KB fixo, igual ao Mapper 000
            mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool Mapper003::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Escrita na ROM seleciona o banco de CHR (normalmente os 2 bits baixos)
            nCHRBankSelect = data & 0x03;
        }

        // Retornamos false porque não há escrita real em memória física (RAM/ROM) aqui
        return false;
    }

    bool Mapper003::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            // Mapeia o banco de 8KB selecionado
            mapped_addr = nCHRBankSelect * 0x2000 + addr;
            return true;
        }
        return false;
    }

    bool Mapper003::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    MirrorMode Mapper003::getMirrorMode()
    {
        // Retorna o modo definido no hardware (passado via constructor)
        return mirrorMode;
    }

    void Mapper003::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
    }

    void Mapper003::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
    }
}
