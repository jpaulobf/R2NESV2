#include "Core/Cartridge/Mappers/Mapper052.h"
#include <cstring>

namespace R2NES::Core
{
    static Mapper004::Configuration makeMmc3Config(uint32_t size, bool battery)
    {
        Mapper004::Configuration c;
        c.prgRamSize = battery ? 0 : size;
        c.prgNvRamSize = battery ? size : 0;
        return c;
    }
    Mapper052::Mapper052(uint16_t prg, uint16_t chr, MirrorMode mirror, uint32_t ramSize, bool battery)
        : Mapper004(16, 32, mirror, makeMmc3Config(ramSize, battery)), totalPrg(prg), totalChr(chr) { reset(); }

    void Mapper052::reset()
    {
        Mapper004::reset();
        outer = 0;
    }
    bool Mapper052::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data)
    {
        if (!Mapper004::cpuMapRead(addr, mapped, data))
            return false;
        if (addr >= 0x8000)
        {
            const unsigned mask = (outer & 8) ? 0x1FFFF : 0x3FFFF;
            const unsigned bank = unsigned(outer & 7) << 17;
            mapped = ((mapped & mask) | (bank & ~mask)) % (unsigned(totalPrg) * 0x4000);
        }
        return true;
    }
    bool Mapper052::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        if (addr >= 0x6000 && addr < 0x8000 && isPrgRamWritable() && !(outer & 0x80))
            outer = data;
        return Mapper004::cpuMapWrite(addr, mapped, data, clock);
    }
    bool Mapper052::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock)
    {
        if (!Mapper004::ppuMapRead(addr, mapped, data, clock))
            return false;
        if (addr < 0x2000)
        {
            const unsigned mask = (outer & 0x40) ? 0x1FFFF : 0x3FFFF;
            const unsigned bank = (unsigned(outer & 0x30) << 13) | (unsigned(outer & 4) << 17);
            mapped = ((mapped & mask) | (bank & ~mask)) % (unsigned(totalChr) * 0x2000);
        }
        return true;
    }
    void Mapper052::saveState(std::ostream &os)
    {
        Mapper004::saveState(os);
        os.write("M052", 4);
        os.put(char(outer));
    }
    void Mapper052::loadState(std::istream &is)
    {
        auto next = *this;
        next.Mapper004::loadState(is);
        char tag[4]{};
        is.read(tag, 4);
        const int value = is.get();
        if (!is || std::memcmp(tag, "M052", 4))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.outer = uint8_t(value);
        *this = next;
    }
}