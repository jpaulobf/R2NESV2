#include "Core/Cartridge/Mappers/Mapper189.h"

namespace R2NES::Core
{
    Mapper189::Mapper189(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror)
        : Mapper004(prgBanks, chrBanks, mirror)
    {
        reset();
    }

    void Mapper189::reset()
    {
        Mapper004::reset();
        prgBank = 0;
    }

    bool Mapper189::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        // Não há PRG RAM: a faixa inferior de registradores aceita apenas escritas.
        if (addr < 0x8000 || nPRGBanks == 0)
            return false;
        mapped_addr = (uint32_t(prgBank) * 0x8000 + (addr & 0x7FFF)) %
                      (uint32_t(nPRGBanks) * 0x4000);
        return true;
    }

    bool Mapper189::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                                uint32_t systemClockCounter)
    {
        // A decodificação generalizada atende às placas originais e às placas piratas genéricas.
        // Consulte https://www.nesdev.org/wiki/INES_Mapper_189
        if (addr >= 0x4020 && addr <= 0x7FFF)
        {
            prgBank = (data >> 4) | (data & 0x0F);
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x8000)
        {
            Mapper004::cpuMapWrite(addr, mapped_addr, data, systemClockCounter);
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        return false;
    }

    void Mapper189::saveState(std::ostream &os)
    {
        Mapper004::saveState(os);
        os.write(reinterpret_cast<const char *>(&prgBank), sizeof(prgBank));
    }

    void Mapper189::loadState(std::istream &is)
    {
        Mapper004::loadState(is);
        is.read(reinterpret_cast<char *>(&prgBank), sizeof(prgBank));
    }
}