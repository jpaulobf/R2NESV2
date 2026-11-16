#include "Core/Cartridge/Mappers/Mapper202.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper202::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        const bool validPrg = prg && prg <= 8 && !(prg & (prg - 1));
        return validPrg && chr <= 8 &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper202::Mapper202(uint16_t prg, uint16_t chr, MirrorMode mirror)
        : Mapper(prg, chr), initialMirrorMode(mirror)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 202 configuration");
        reset();
    }

    bool Mapper202::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &)
    {
        if (addr < 0x8000)
            return false;
        const unsigned bank = addressLatch == 0xFF ? 0 : (addressLatch >> 1) & 7;
        const bool sequential = addressLatch != 0xFF && (addressLatch & 9) == 9;
        const unsigned selected = (bank + (sequential && addr >= 0xC000 ? 1 : 0)) % nPRGBanks;
        mapped = selected * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    bool Mapper202::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr < 0x8000)
            return false;
        addressLatch = static_cast<uint8_t>(addr & 0x0F);
        mapped = 0xFFFFFFFF;
        return true;
    }

    bool Mapper202::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (addr >= 0x2000)
            return false;
        const unsigned bank = addressLatch == 0xFF ? 0 : (addressLatch >> 1) & 7;
        // O dump conhecido tem sete bancos CHR; usa-se o mesmo espelhamento
        // por módulo do Mesen para um banco selecionado fora da imagem.
        mapped = (nCHRBanks ? bank % nCHRBanks : 0) * 0x2000 + addr;
        return true;
    }

    bool Mapper202::ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t, uint32_t)
    {
        if (addr >= 0x2000 || nCHRBanks != 0)
            return false;
        mapped = addr;
        return true;
    }

    MirrorMode Mapper202::getMirrorMode()
    {
        return addressLatch == 0xFF ? initialMirrorMode
                                    : (addressLatch & 1 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL);
    }

    void Mapper202::reset() { addressLatch = 0xFF; }

    void Mapper202::saveState(std::ostream &os) { os.put(static_cast<char>(addressLatch)); }

    void Mapper202::loadState(std::istream &is)
    {
        const int saved = is.get();
        if (saved == std::char_traits<char>::eof() || (saved > 0x0F && saved != 0xFF))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        addressLatch = static_cast<uint8_t>(saved);
    }
}