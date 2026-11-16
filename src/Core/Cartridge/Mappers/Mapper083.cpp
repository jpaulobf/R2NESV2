#include "Core/Cartridge/Mappers/Mapper083.h"
#include <cstring>

namespace R2NES::Core
{
    Mapper083::Mapper083(uint16_t p, uint16_t c, bool large, MirrorMode mirror)
        : Mapper(p, c), largeChr(large), initialMirror(mirror) { reset(); }
    void Mapper083::reset()
    {
        base = source = 0;
        mode = initialMirror == MirrorMode::HORIZONTAL ? 1 : 0;
        prg.fill(0);
        chr.fill(0);
        scratch.fill(0);
        counter = 0;
        irqEnabled = irqPending = a12 = false;
    }
    bool Mapper083::cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &data)
    {
        if ((a & 0xF100) == 0x5000)
        {
            data &= 0xFC;
            mapped = 0xFFFFFFFF;
            return true;
        }
        if ((a & 0xF100) == 0x5100)
        {
            data = scratch[a & 3];
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (a < 0x6000 || (a < 0x8000 && !(mode & 0x20)))
            return false;
        const unsigned region = unsigned(base) << 14;
        unsigned offset;
        if (a < 0x8000)
            offset = (region & ~0x3FFFFu) | (unsigned(prg[3] & 31) << 13) | (a & 0x1FFF);
        else
        {
            const unsigned banking = (mode >> 3) & 3;
            if (banking == 0)
            {
                const unsigned selected = a < 0xC000 ? base : ((base & ~15u) | 15u);
                offset = (selected << 14) | (a & 0x3FFF);
            }
            else if (banking == 1)
                offset = region | (a & 0x3FFF);
            else
            {
                const unsigned slot = (a - 0x8000) >> 13;
                const unsigned bank = slot == 3 ? 31 : (prg[slot] & 31);
                offset = (region & ~0x3FFFFu) | (bank << 13) | (a & 0x1FFF);
            }
        }
        mapped = offset % (unsigned(nPRGBanks) * 0x4000);
        return true;
    }
    bool Mapper083::cpuMapWrite(uint16_t a, uint32_t &mapped, uint8_t value, uint32_t)
    {
        if ((a & 0xF100) == 0x5100)
            scratch[a & 3] = value;
        else if (a >= 0x8000)
        {
            switch (a & 0x8300)
            {
            case 0x8000:
                base = value;
                break;
            case 0x8100:
                mode = value;
                break;
            case 0x8200:
                if (a & 1)
                {
                    counter = (counter & 0xFF) | (uint16_t(value) << 8);
                    irqEnabled = (mode & 0x80) != 0;
                }
                else
                {
                    counter = (counter & 0xFF00) | value;
                    irqPending = false;
                }
                break;
            case 0x8300:
                if (!(a & 0x10))
                    prg[a & 3] = value;
                else if (!(a & 8))
                    chr[a & 7] = value;
                else
                    source = value;
                break;
            }
        }
        else
            return false;
        mapped = 0xFFFFFFFF;
        return true;
    }
    bool Mapper083::ppuMapRead(uint16_t a, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (a >= 0x2000)
            return false;
        if (largeChr)
        {
            const unsigned indices[4] = {0, 1, 6, 7};
            mapped = (unsigned(chr[indices[a >> 11]]) * 0x800 + (a & 0x7FF)) % (unsigned(nCHRBanks) * 0x2000);
        }
        else
            mapped = (unsigned(chr[a >> 10]) * 0x400 + (a & 0x3FF)) % (unsigned(nCHRBanks) * 0x2000);
        return true;
    }
    MirrorMode Mapper083::getMirrorMode()
    {
        const MirrorMode modes[] = {MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_LO, MirrorMode::ONESCREEN_HI};
        return modes[mode & 3];
    }
    void Mapper083::clockIrq()
    {
        if (!irqEnabled || !counter)
            return;
        counter = uint16_t(counter + ((mode & 0x40) ? -1 : 1));
        if (!counter)
        {
            irqEnabled = false;
            irqPending = true;
        }
    }
    void Mapper083::tick()
    {
        if (!source)
            clockIrq();
    }
    void Mapper083::onPpuAddress(uint16_t addr, uint32_t)
    {
        const bool high = (addr & 0x1000) != 0;
        if (source && high && !a12)
            clockIrq();
        a12 = high;
    }
    void Mapper083::saveState(std::ostream &os)
    {
        os.write("C083", 4);
        os.put(char(base));
        os.put(char(mode));
        os.put(char(source));
        os.write(reinterpret_cast<const char *>(prg.data()), prg.size());
        os.write(reinterpret_cast<const char *>(chr.data()), chr.size());
        os.write(reinterpret_cast<const char *>(scratch.data()), scratch.size());
        os.put(char(counter));
        os.put(char(counter >> 8));
        os.put(char(irqEnabled));
        os.put(char(irqPending));
        os.put(char(a12));
    }
    void Mapper083::loadState(std::istream &is)
    {
        auto n = *this;
        char sig[4]{};
        is.read(sig, 4);
        int b = is.get(), m = is.get(), s = is.get();
        is.read(reinterpret_cast<char *>(n.prg.data()), n.prg.size());
        is.read(reinterpret_cast<char *>(n.chr.data()), n.chr.size());
        is.read(reinterpret_cast<char *>(n.scratch.data()), n.scratch.size());
        int lo = is.get(), hi = is.get(), enabled = is.get(), pending = is.get(), last = is.get();
        if (!is || std::memcmp(sig, "C083", 4) || enabled > 1 || pending > 1 || last > 1)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        n.base = uint8_t(b);
        n.mode = uint8_t(m);
        n.source = uint8_t(s);
        n.counter = uint16_t(lo | (hi << 8));
        n.irqEnabled = enabled != 0;
        n.irqPending = pending != 0;
        n.a12 = last != 0;
        *this = n;
    }
}