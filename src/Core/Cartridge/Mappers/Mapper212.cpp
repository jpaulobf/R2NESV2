#include "Core/Cartridge/Mappers/Mapper212.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper212::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        const auto powerOfTwo = [](uint16_t value)
        { return value && !(value & (value - 1)); };
        return powerOfTwo(prg) && prg <= 8 &&
               (chr == 0 || (powerOfTwo(chr) && chr <= 8)) &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper212::Mapper212(uint16_t prg, uint16_t chr, MirrorMode mirror) : Mapper(prg, chr)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 212 configuration");
        reset();
    }

    bool Mapper212::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data)
    {
        if (addr >= 0x6000 && addr < 0x8000)
        {
            // D7 inverte A4; D6-D4 ficam abertos e os quatro contatos de
            // seleção do menu usam a configuração padrão zero.
            data = static_cast<uint8_t>((data & 0x70) | ((addr & 0x10) ? 0 : 0x80));
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (addr < 0x8000)
            return false;
        unsigned bank = addressLatch & 7;
        if (addressLatch & 0x10)
            bank = (bank & 6) | ((addr >> 14) & 1);
        bank &= nPRGBanks - 1;
        mapped = bank * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    bool Mapper212::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr < 0x8000)
            return false;
        addressLatch = static_cast<uint8_t>((addr & 0x0F) | ((addr >> 10) & 0x10));
        mapped = 0xFFFFFFFF;
        return true;
    }

    bool Mapper212::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        const unsigned bank = nCHRBanks ? (addressLatch & 7) & (nCHRBanks - 1) : 0;
        mapped = bank * 0x2000 + addr;
        return true;
    }

    bool Mapper212::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr >= 0x2000 || nCHRBanks != 0)
            return false;
        mapped = addr;
        return true;
    }

    MirrorMode Mapper212::getMirrorMode()
    {
        return addressLatch & 8 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
    }

    void Mapper212::reset() { addressLatch = 0; }

    void Mapper212::saveState(std::ostream &os)
    {
        os.put(static_cast<char>(addressLatch));
    }

    void Mapper212::loadState(std::istream &is)
    {
        const int saved = is.get();
        if (saved == std::char_traits<char>::eof() || saved > 0x1F)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        addressLatch = static_cast<uint8_t>(saved);
    }
}