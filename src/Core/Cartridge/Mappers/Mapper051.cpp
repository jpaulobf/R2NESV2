#include "Core/Cartridge/Mappers/Mapper051.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper051::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        return prg && prg <= 32 && !(prg & (prg - 1)) && chr == 0 &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper051::Mapper051(uint16_t prg, uint16_t chr, MirrorMode mirror) : Mapper(prg, chr)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 51 configuration");
        reset();
    }

    bool Mapper051::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &)
    {
        if (addr < 0x6000)
            return false;
        const unsigned base = unsigned(bank) << 2;
        unsigned page;
        if (addr < 0x8000)
            page = base | (mode & 1 ? 0x23 : 0x2F);
        else if (mode & 1)
            page = base + ((addr - 0x8000) >> 13);
        else if (addr < 0xC000)
            page = (base | mode) + ((addr - 0x8000) >> 13);
        else
            page = (base | 0x0E) + ((addr - 0xC000) >> 13);
        mapped = (page % (unsigned(nPRGBanks) * 2)) * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    bool Mapper051::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t value, uint32_t)
    {
        if (addr < 0x6000)
            return false;
        if (addr < 0x8000)
            mode = ((value >> 3) & 2) | ((value >> 1) & 1);
        else
        {
            bank = value & 0x0F;
            if (addr >= 0xC000 && addr < 0xE000)
                mode = ((value >> 3) & 2) | (mode & 1);
        }
        mapped = 0xFFFFFFFF;
        return true;
    }

    bool Mapper051::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        mapped = addr;
        return true;
    }

    bool Mapper051::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        mapped = addr;
        return true;
    }

    MirrorMode Mapper051::getMirrorMode()
    {
        return mode == 3 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
    }
    void Mapper051::reset()
    {
        bank = 0;
        mode = 1;
    }
    void Mapper051::saveState(std::ostream &os)
    {
        os.put(char(bank));
        os.put(char(mode));
    }
    void Mapper051::loadState(std::istream &is)
    {
        const int savedBank = is.get();
        const int savedMode = is.get();
        if (!is || savedBank > 15 || savedMode > 3)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        bank = static_cast<uint8_t>(savedBank);
        mode = static_cast<uint8_t>(savedMode);
    }
}