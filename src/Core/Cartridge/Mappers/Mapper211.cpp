#include "Core/Cartridge/Mappers/Mapper211.h"
#include <algorithm>
#include <cstring>

namespace R2NES::Core
{
    Mapper211::Mapper211(uint16_t prg, uint16_t chr) : Mapper090(prg, chr) { reset(); }

    void Mapper211::reset()
    {
        Mapper090::reset();
        std::fill(std::begin(ntLow), std::end(ntLow), 0);
        std::fill(std::begin(ntHigh), std::end(ntHigh), 0);
        mirrorControl = ntCompare = 0;
        chrLatch[0] = 0;
        chrLatch[1] = 4;
        // CIRAM is console memory and survives a mapper reset.
    }

    bool Mapper211::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        if (addr >= 0x8000 && (addr & 0x0800))
        {
            if ((irqMode & 3) == 3)
                clockIrq();
            return false;
        }
        if ((addr & 0xF000) == 0xB000)
        {
            if ((irqMode & 3) == 3)
                clockIrq();
            if (addr & 4)
                ntHigh[addr & 3] = data;
            else
                ntLow[addr & 3] = data;
            mapped = 0xFFFFFFFF;
            return true;
        }
        if ((addr & 0xF000) == 0xD000)
        {
            addr &= ~uint16_t(4);
            if ((addr & 3) == 1)
                mirrorControl = data & 0x0B;
            if ((addr & 3) == 2)
                ntCompare = data & 0x80;
        }
        return Mapper090::cpuMapWrite(addr, mapped, data, clock);
    }

    uint16_t Mapper211::ciramAddress(uint16_t addr) const
    {
        const unsigned nt = (addr >> 10) & 3;
        if ((mode & 0x20) || (mirrorControl & 0x08))
            return (ntLow[nt] & 1) * 0x400 + (addr & 0x3FF);
        switch (mirrorControl & 3)
        {
        case 0:
            return addr & 0x7FF;
        case 1:
            return (addr & 0x3FF) | ((addr & 0x800) >> 1);
        case 2:
            return addr & 0x3FF;
        default:
            return (addr & 0x3FF) | 0x400;
        }
    }

    bool Mapper211::mapNametable(uint16_t addr, uint32_t &mapped, uint8_t &data) const
    {
        if (addr < 0x2000 || addr >= 0x3F00)
            return false;
        const unsigned nt = (addr >> 10) & 3;
        const bool rom = (mode & 0x20) && ((mode & 0x40) || ((ntLow[nt] ^ ntCompare) & 0x80));
        if (rom && nCHRBanks)
        {
            const unsigned page = ntLow[nt] | (unsigned(ntHigh[nt]) << 8);
            mapped = (page % (unsigned(nCHRBanks) * 8)) * 0x400 + (addr & 0x3FF);
        }
        else
        {
            data = ciram.readRaw(ciramAddress(addr));
            mapped = 0xFFFFFFFF;
        }
        return true;
    }

    bool Mapper211::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock)
    {
        if (mapNametable(addr, mapped, data))
            return true;
        return Mapper090::ppuMapRead(addr, mapped, data, clock);
    }
    bool Mapper211::ppuMapPeek(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock)
    {
        if (mapNametable(addr, mapped, data))
            return true;
        return Mapper090::ppuMapPeek(addr, mapped, data, clock);
    }
    bool Mapper211::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        if (addr >= 0x2000 && addr < 0x3F00)
        {
            ciram.writeRaw(ciramAddress(addr), data);
            mapped = 0xFFFFFFFF;
            return true;
        }
        return Mapper090::ppuMapWrite(addr, mapped, data, clock);
    }

    uint32_t Mapper211::chrAddress(uint16_t addr) const
    {
        const unsigned cm = (mode >> 3) & 3, slot = addr >> 10;
        unsigned index, offset;
        switch (cm)
        {
        case 0:
            index = 0;
            offset = slot;
            break;
        case 1:
            index = (block & 0x80) ? chrLatch[slot >> 2] : (slot & 4);
            offset = slot & 3;
            break;
        case 2:
            index = slot & 6;
            offset = slot & 1;
            break;
        default:
            index = slot;
            offset = 0;
            break;
        }
        unsigned reg = chrLow[index] | (unsigned(chrHigh[index]) << 8);
        if (!(block & 0x20))
        {
            const unsigned shift = 5 + cm;
            reg = (chrLow[index] & ((1u << shift) - 1)) | ((((block & 0x18) >> 2) | (block & 1)) << shift);
        }
        const unsigned count = nCHRBanks ? unsigned(nCHRBanks) * 8 : 8;
        return (((reg << (3 - cm)) + offset) % count) * 0x400 + (addr & 0x3FF);
    }

    void Mapper211::onPpuAddress(uint16_t addr, uint32_t clock)
    {
        Mapper090::onPpuAddress(addr, clock);
        if ((block & 0x80) && ((mode >> 3) & 3) == 1)
        {
            const uint16_t trigger = addr & 0x2FF8;
            if (trigger == 0x0FD8 || trigger == 0x0FE8)
                chrLatch[(addr >> 12) & 1] = uint8_t(((addr >> 12) & 1) * 4 + (trigger == 0x0FE8 ? 2 : 0));
        }
    }

    void Mapper211::saveState(std::ostream &os)
    {
        Mapper090::saveState(os);
        os.write("JY11", 4);
        os.write(reinterpret_cast<const char *>(ntLow), sizeof(ntLow));
        os.write(reinterpret_cast<const char *>(ntHigh), sizeof(ntHigh));
        os.put(char(mirrorControl));
        os.put(char(ntCompare));
        os.put(char(chrLatch[0]));
        os.put(char(chrLatch[1]));
        ciram.saveState(os);
    }
    void Mapper211::loadState(std::istream &is)
    {
        Mapper211 next(*this);
        next.Mapper090::loadState(is);
        char signature[4]{};
        is.read(signature, 4);
        is.read(reinterpret_cast<char *>(next.ntLow), sizeof(ntLow));
        is.read(reinterpret_cast<char *>(next.ntHigh), sizeof(ntHigh));
        const int mirror = is.get(), compare = is.get(), l0 = is.get(), l1 = is.get();
        next.ciram.loadState(is);
        if (!is || std::memcmp(signature, "JY11", 4) || (mirror & ~0x0B) || (compare != 0 && compare != 0x80) ||
            (l0 != 0 && l0 != 2) || (l1 != 4 && l1 != 6))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.mirrorControl = uint8_t(mirror);
        next.ntCompare = uint8_t(compare);
        next.chrLatch[0] = uint8_t(l0);
        next.chrLatch[1] = uint8_t(l1);
        *this = next;
    }
}