#include "Core/Cartridge/Mappers/Mapper091.h"
#include <cstring>

namespace R2NES::Core
{
    Mapper091::Mapper091(uint16_t p, uint16_t c, MirrorMode m) : Mapper(p, c), mirror(m) { reset(); }
    void Mapper091::reset()
    {
        prg.fill(0);
        chr.fill(0);
        outer = irqCounter = 0;
        irqEnabled = irqPending = lastA12 = false;
    }
    bool Mapper091::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &)
    {
        if (addr < 0x8000)
            return false;
        unsigned slot = (addr - 0x8000) >> 13;
        unsigned bank = (unsigned(outer & 6) << 3) | (slot < 2 ? (prg[slot] & 15) : (14 + slot - 2));
        mapped = (bank % (unsigned(nPRGBanks) * 2)) * 0x2000 + (addr & 0x1FFF);
        return true;
    }
    bool Mapper091::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t)
    {
        if (addr >= 0x6000 && addr < 0x7000)
            chr[addr & 3] = data;
        else if (addr >= 0x7000 && addr < 0x8000)
        {
            switch (addr & 3)
            {
            case 0:
            case 1:
                prg[addr & 1] = data & 15;
                break;
            case 2:
                irqEnabled = irqPending = false;
                break;
            case 3:
                irqCounter = 0;
                irqEnabled = true;
                break;
            }
        }
        else if (addr >= 0x8000 && addr < 0xA000)
            outer = data & 7;
        else
            return false;
        mapped = 0xFFFFFFFF;
        return true;
    }
    bool Mapper091::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        const unsigned bank = (unsigned(outer & 1) << 8) | chr[addr >> 11];
        mapped = (bank % (unsigned(nCHRBanks) * 4)) * 0x800 + (addr & 0x7FF);
        return true;
    }
    void Mapper091::onPpuAddress(uint16_t addr, uint32_t)
    {
        bool a12 = (addr & 0x1000) != 0;
        if (a12 && !lastA12 && irqEnabled && irqCounter < 64 && ++irqCounter == 64)
            irqPending = true;
        lastA12 = a12;
    }
    void Mapper091::saveState(std::ostream &os)
    {
        os.write("HK91", 4);
        os.write(reinterpret_cast<const char *>(prg.data()), prg.size());
        os.write(reinterpret_cast<const char *>(chr.data()), chr.size());
        os.put(char(outer));
        os.put(char(irqCounter));
        os.put(char(irqEnabled));
        os.put(char(irqPending));
        os.put(char(lastA12));
    }
    void Mapper091::loadState(std::istream &is)
    {
        auto next = *this;
        char signature[4]{};
        is.read(signature, 4);
        is.read(reinterpret_cast<char *>(next.prg.data()), next.prg.size());
        is.read(reinterpret_cast<char *>(next.chr.data()), next.chr.size());
        int out = is.get(), count = is.get(), enabled = is.get(), pending = is.get(), a12 = is.get();
        if (!is || std::memcmp(signature, "HK91", 4) || out > 7 || count > 64 ||
            enabled > 1 || pending > 1 || a12 > 1 || (pending && !enabled) ||
            next.prg[0] > 15 || next.prg[1] > 15)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.outer = uint8_t(out);
        next.irqCounter = uint8_t(count);
        next.irqEnabled = enabled != 0;
        next.irqPending = pending != 0;
        next.lastA12 = a12 != 0;
        *this = next;
    }
}