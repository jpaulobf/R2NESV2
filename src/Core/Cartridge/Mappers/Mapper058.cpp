#include "Core/Cartridge/Mappers/Mapper058.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper058::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        const bool validPrg = prg && prg <= 8 && !(prg & (prg - 1));
        return validPrg && (chr == 0 || (chr <= 8 && !(chr & (chr - 1)))) &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper058::Mapper058(uint16_t prg, uint16_t chr, MirrorMode mirror)
        : Mapper(prg, chr), initialMirrorMode(mirror)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 58 configuration");
        reset();
    }

    bool Mapper058::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &)
    {
        if (addr < 0x8000)
            return false;
        const unsigned bank = addressLatch == 0x100 ? 0 : addressLatch & 7;
        const bool mirrored = addressLatch != 0x100 && (addressLatch & 0x40);
        const unsigned selected = (mirrored ? bank : ((bank & 6) | (addr >= 0xC000 ? 1 : 0))) % nPRGBanks;
        mapped = selected * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    bool Mapper058::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr < 0x8000)
            return false;
        addressLatch = static_cast<uint8_t>(addr & 0xFF);
        mapped = 0xFFFFFFFF;
        return true;
    }

    bool Mapper058::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        const unsigned bank = addressLatch == 0x100 ? 0 : (addressLatch >> 3) & 7;
        mapped = (nCHRBanks ? bank % nCHRBanks : 0) * 0x2000 + addr;
        return true;
    }

    bool Mapper058::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr >= 0x2000 || nCHRBanks != 0)
            return false;
        mapped = addr;
        return true;
    }

    MirrorMode Mapper058::getMirrorMode()
    {
        return addressLatch == 0x100 ? initialMirrorMode
                                     : (addressLatch & 0x80 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL);
    }

    void Mapper058::reset() { addressLatch = 0x100; }

    void Mapper058::saveState(std::ostream &os)
    {
        os.put(static_cast<char>(addressLatch & 0xFF));
        os.put(static_cast<char>(addressLatch >> 8));
    }

    void Mapper058::loadState(std::istream &is)
    {
        const int low = is.get();
        const int high = is.get();
        if (!is || high > 1 || (high == 1 && low != 0))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        addressLatch = static_cast<uint16_t>(low | (high << 8));
    }
}