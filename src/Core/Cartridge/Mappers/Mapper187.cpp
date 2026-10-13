#include "Core/Cartridge/Mappers/Mapper187.h"

namespace R2NES::Core
{
    Mapper187::Mapper187(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror)
        : Mapper004(prgBanks, chrBanks, mirror)
    {
        reset();
    }

    void Mapper187::reset()
    {
        Mapper004::reset();
        nromModeRegister = 0;
    }

    bool Mapper187::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && (nromModeRegister & 0x80))
        {
            // O bit 7 de $5000 ativa o mapeamento NROM. Os bits inferiores selecionam
            // um banco de 16 KiB; o bit 5 seleciona o modo NROM-256, conectando A14
            // da CPU a A14 da PRG. Com o bit 5 desativado, o banco de 16 KiB é espelhado.
            const uint32_t total16kBanks = nPRGBanks;
            if (total16kBanks == 0)
                return false;

            uint32_t bank16 = nromModeRegister & 0x0F;
            const uint32_t cpuA14 = (addr >> 14) & 0x01;
            if (nromModeRegister & 0x20)
                bank16 = (bank16 & ~1u) | cpuA14;
            bank16 %= total16kBanks;
            mapped_addr = bank16 * 0x4000 + (addr & 0x3FFF);
            return true;
        }

        return Mapper004::cpuMapRead(addr, mapped_addr, data);
    }

    bool Mapper187::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                                uint32_t systemClockCounter)
    {
        // O registrador de mapeamento NROM é decodificado em $5000 (os bits inferiores
        // do endereço não estão conectados na placa).
        if ((addr & 0xF007) == 0x5000)
        {
            nromModeRegister = data;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }

        return Mapper004::cpuMapWrite(addr, mapped_addr, data, systemClockCounter);
    }

    bool Mapper187::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data,
                               uint32_t systemClockCounter)
    {
        const bool mapped = Mapper004::ppuMapRead(addr, mapped_addr, data, systemClockCounter);
        if (mapped && nCHRBanks > 32)
            mapped_addr = (mapped_addr & 0x3FFFF) | ((addr & 0x1000) ? 0x40000 : 0);
        return mapped;
    }

    bool Mapper187::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                                uint32_t systemClockCounter)
    {
        const bool mapped = Mapper004::ppuMapWrite(addr, mapped_addr, data, systemClockCounter);
        if (mapped && nCHRBanks > 32)
            mapped_addr = (mapped_addr & 0x3FFFF) | ((addr & 0x1000) ? 0x40000 : 0);
        return mapped;
    }

    void Mapper187::saveState(std::ostream &os)
    {
        Mapper004::saveState(os);
        os.write(reinterpret_cast<const char *>(&nromModeRegister), sizeof(nromModeRegister));
    }

    void Mapper187::loadState(std::istream &is)
    {
        Mapper004::loadState(is);
        is.read(reinterpret_cast<char *>(&nromModeRegister), sizeof(nromModeRegister));
    }
}