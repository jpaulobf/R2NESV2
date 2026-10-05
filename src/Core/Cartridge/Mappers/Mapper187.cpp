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
            // $5000 bit 7 selects the NROM override. The low bank bits choose
            // a 16 KiB bank; bit 5 selects NROM-256 behavior by routing CPU A14
            // to PRG A14. With bit 5 clear, the selected 16 KiB bank is mirrored.
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
        // The NROM override register is decoded at $5000 (low address bits
        // are not connected on the board).
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
} // namespace R2NES::Core
