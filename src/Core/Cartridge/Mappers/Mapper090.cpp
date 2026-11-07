#include "Core/Cartridge/Mappers/Mapper090.h"
#include "Core/Memory/StateRead.h"
#include <algorithm>
#include <cstring>

namespace R2NES::Core
{
    Mapper090::Mapper090(uint16_t p, uint16_t c) : Mapper(p, c) { reset(); }
    void Mapper090::reset()
    {
        std::fill(std::begin(prg), std::end(prg), 0);
        std::fill(std::begin(chrLow), std::end(chrLow), 0);
        std::fill(std::begin(chrHigh), std::end(chrHigh), 0);
        mode = mirror = multA = multB = scratch = irqMode = irqPrescaler = irqCounter = irqXor = irqExtra = 0;
        block = 0x20;
        irqEnabled = irqPending = false;
        lastPpuAddress = 0;
    }
    bool Mapper090::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data)
    {
        if (addr >= 0x5000 && addr < 0x6000)
        {
            switch (addr & 0xF803)
            {
            case 0x5000:
                data = 0;
                break;
            case 0x5800:
                data = uint8_t(unsigned(multA) * multB);
                break;
            case 0x5801:
                data = uint8_t((unsigned(multA) * multB) >> 8);
                break;
            case 0x5803:
                data = scratch;
                break;
            default:
                return false;
            }
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (addr < 0x6000 || (addr < 0x8000 && !(mode & 0x80)))
            return false;
        unsigned regs[4];
        for (unsigned i = 0; i < 4; ++i)
        {
            unsigned v = prg[i];
            if ((mode & 3) == 3)
                v = ((v & 1) << 6) | ((v & 2) << 4) | ((v & 4) << 2) | (v & 8) | ((v & 16) >> 2) | ((v & 32) >> 4) | ((v & 64) >> 6);
            regs[i] = v;
        }
        unsigned bank, slot = (addr - 0x8000u) / 0x2000u;
        switch (mode & 3)
        {
        case 0:
            bank = addr < 0x8000 ? regs[3] * 4 + 3 : ((mode & 4) ? (regs[3] & ~3u) : 0x3c) + slot;
            break;
        case 1:
            bank = addr < 0x8000 ? regs[3] * 2 + 1 : slot < 2 ? regs[1] * 2 + slot
                                                              : ((mode & 4) ? (regs[3] & ~1u) : 0x3e) + slot - 2;
            break;
        default:
            bank = addr < 0x8000 ? regs[3] : slot == 3 && !(mode & 4) ? 0x3f
                                                                      : regs[slot];
            break;
        }
        unsigned count = std::max(1u, unsigned(nPRGBanks) * 2);
        mapped = (bank % count) * 0x2000 + (addr & 0x1fff);
        return true;
    }
    bool Mapper090::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t)
    {
        if ((irqMode & 3) == 3)
            clockIrq();
        if (addr >= 0x5000 && addr < 0x6000)
        {
            switch (addr & 0xF803)
            {
            case 0x5800:
                multA = data;
                break;
            case 0x5801:
                multB = data;
                break;
            case 0x5803:
                scratch = data;
                break;
            default:
                return false;
            }
        }
        else if (addr >= 0x8000)
        {
            switch (addr & 0xF000)
            {
            case 0x8000:
                prg[addr & 3] = data & 0x7f;
                break;
            case 0x9000:
                chrLow[addr & 7] = data;
                break;
            case 0xA000:
                chrHigh[addr & 7] = data;
                break;
            case 0xC000:
                switch (addr & 7)
                {
                case 0:
                    irqEnabled = data & 1;
                    if (!irqEnabled)
                        irqPending = false;
                    break;
                case 1:
                    irqMode = data;
                    break;
                case 2:
                    irqEnabled = irqPending = false;
                    break;
                case 3:
                    irqEnabled = true;
                    break;
                case 4:
                    irqPrescaler = data ^ irqXor;
                    break;
                case 5:
                    irqCounter = data ^ irqXor;
                    break;
                case 6:
                    irqXor = data;
                    break;
                case 7:
                    irqExtra = data;
                    break;
                }
                break;
            case 0xD000:
                switch (addr & 7)
                {
                case 0:
                    mode = data;
                    break;
                case 1:
                    mirror = data & 3;
                    break;
                case 3:
                    block = data;
                    break;
                }
                break;
            default:
                break;
            }
        }
        else
            return false;
        mapped = 0xFFFFFFFF;
        return true;
    }
    uint32_t Mapper090::chrAddress(uint16_t addr) const
    {
        unsigned cm = (mode >> 3) & 3, slot = addr >> 10, index = 0, offset = 0;
        switch (cm)
        {
        case 0:
            index = 0;
            offset = slot;
            break;
        case 1:
            index = slot & 4;
            offset = slot & 3;
            break;
        case 2:
            index = slot & 6;
            offset = slot & 1;
            break;
        default:
            index = slot;
        }
        if (cm >= 2 && (block & 0x80) && (index == 2 || index == 3))
            index -= 2;
        unsigned reg = chrLow[index] | (unsigned(chrHigh[index]) << 8);
        if (!(block & 0x20))
        {
            unsigned shift = 5 + cm;
            unsigned b = ((block & 0x18) >> 2) | (block & 1);
            reg = (chrLow[index] & ((1u << shift) - 1)) | (b << shift);
        }
        unsigned bank = (reg << (3 - cm)) + offset;
        unsigned count = nCHRBanks ? unsigned(nCHRBanks) * 8 : 8;
        return (bank % count) * 0x400 + (addr & 0x3ff);
    }
    bool Mapper090::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        mapped = chrAddress(addr);
        return true;
    }
    bool Mapper090::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr >= 0x2000 || nCHRBanks)
            return false;
        mapped = chrAddress(addr);
        return true;
    }
    MirrorMode Mapper090::getMirrorMode()
    {
        switch (mirror)
        {
        case 0:
            return MirrorMode::VERTICAL;
        case 1:
            return MirrorMode::HORIZONTAL;
        case 2:
            return MirrorMode::ONESCREEN_LO;
        default:
            return MirrorMode::ONESCREEN_HI;
        }
    }
    void Mapper090::clockIrq()
    {
        uint8_t mask = (irqMode & 4) ? 7 : 255, p = irqPrescaler & mask;
        unsigned dir = irqMode >> 6;
        bool clock = false;
        if (dir == 1)
        {
            ++p;
            clock = (p & mask) == 0;
        }
        else if (dir == 2)
        {
            --p;
            clock = p == 0;
        }
        irqPrescaler = (irqPrescaler & ~mask) | (p & mask);
        if (clock)
        {
            if (dir == 1)
            {
                ++irqCounter;
                if (!irqCounter && irqEnabled)
                    irqPending = true;
            }
            else
            {
                --irqCounter;
                if (irqCounter == 255 && irqEnabled)
                    irqPending = true;
            }
        }
    }
    void Mapper090::tick()
    {
        if ((irqMode & 3) == 0)
            clockIrq();
    }
    void Mapper090::onPpuAddress(uint16_t addr, uint32_t)
    {
        if ((irqMode & 3) == 1 && (addr & 0x1000) && !(lastPpuAddress & 0x1000))
            clockIrq();
        lastPpuAddress = addr;
    }
    void Mapper090::onPpuRead(uint16_t)
    {
        if ((irqMode & 3) == 2)
            clockIrq();
    }
    void Mapper090::saveState(std::ostream &os)
    {
        os.write("JY90", 4);
        os.write(reinterpret_cast<const char *>(prg), sizeof(prg));
        os.write(reinterpret_cast<const char *>(chrLow), sizeof(chrLow));
        os.write(reinterpret_cast<const char *>(chrHigh), sizeof(chrHigh));
        const uint8_t values[] = {mode, mirror, block, multA, multB, scratch, irqMode, irqPrescaler, irqCounter, irqXor, irqExtra};
        os.write(reinterpret_cast<const char *>(values), sizeof(values));
        os.write(reinterpret_cast<const char *>(&irqEnabled), sizeof(irqEnabled));
        os.write(reinterpret_cast<const char *>(&irqPending), sizeof(irqPending));
        os.write(reinterpret_cast<const char *>(&lastPpuAddress), sizeof(lastPpuAddress));
    }
    void Mapper090::loadState(std::istream &is)
    {
        Mapper090 next(*this);
        uint8_t v[11]{};
        char signature[4]{};
        is.read(signature, sizeof(signature));
        if (!is || std::memcmp(signature, "JY90", 4) != 0)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        is.read(reinterpret_cast<char *>(next.prg), sizeof(prg));
        is.read(reinterpret_cast<char *>(next.chrLow), sizeof(chrLow));
        is.read(reinterpret_cast<char *>(next.chrHigh), sizeof(chrHigh));
        is.read(reinterpret_cast<char *>(v), sizeof(v));
        readStateBool(is, next.irqEnabled);
        readStateBool(is, next.irqPending);
        is.read(reinterpret_cast<char *>(&next.lastPpuAddress), sizeof(next.lastPpuAddress));
        if (!is || v[1] > 3 || next.lastPpuAddress > 0x3fff || std::any_of(std::begin(next.prg), std::end(next.prg), [](uint8_t b)
                                                                           { return b > 0x7f; }))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.mode = v[0];
        next.mirror = v[1];
        next.block = v[2];
        next.multA = v[3];
        next.multB = v[4];
        next.scratch = v[5];
        next.irqMode = v[6];
        next.irqPrescaler = v[7];
        next.irqCounter = v[8];
        next.irqXor = v[9];
        next.irqExtra = v[10];
        *this = next;
    }
}