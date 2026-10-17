#include "Core/Cartridge/Mappers/Mapper150.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper150::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        return prg >= 2 && prg <= 8 && (prg & (prg - 1)) == 0 &&
               chr >= 1 && chr <= 8 && (chr & (chr - 1)) == 0 &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }
    Mapper150::Mapper150(uint16_t prg, uint16_t chr, MirrorMode mirror) : Mapper(prg, chr)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 150 configuration");
        reset();
    }
    bool Mapper150::cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data)
    {
        if ((addr & 0xC101) == 0x4101)
        {
            // A placa dirige apenas D0-D2; D3-D7 conservam o barramento aberto.
            data = (data & 0xF8) | registers[index];
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (addr < 0x8000)
            return false;
        mapped = (registers[5] & (nPRGBanks / 2 - 1)) * 32768u + (addr & 0x7FFF);
        return true;
    }
    bool Mapper150::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        const uint16_t port = addr & 0xC101;
        if (port != 0x4100 && port != 0x4101)
            return false;
        if (port == 0x4100)
            index = data & 7;
        else
            registers[index] = data & 7;
        mapped = 0xFFFFFFFF;
        return true;
    }
    bool Mapper150::ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock)
    {
        if (addr > 0x1FFF)
            return false;
        const unsigned bank = ((registers[4] & 1) << 2) | (registers[6] & 3);
        mapped = (bank & (nCHRBanks - 1)) * 8192u + addr;
        return true;
    }
    bool Mapper150::ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) { return false; }
    MirrorMode Mapper150::getMirrorMode()
    {
        switch ((registers[7] >> 1) & 3)
        {
        case 0:
            return MirrorMode::SACHEN_L;
        case 1:
            return MirrorMode::HORIZONTAL;
        case 2:
            return MirrorMode::VERTICAL;
        default:
            return MirrorMode::ONESCREEN_HI;
        }
    }
    void Mapper150::reset()
    {
        // Inicialização determinística do emulador; não é um reset elétrico da placa.
        index = 0;
        registers.fill(0);
    }
    void Mapper150::saveState(std::ostream &os)
    {
        os.put(static_cast<char>(index));
        os.write(reinterpret_cast<const char *>(registers.data()), registers.size());
    }
    void Mapper150::loadState(std::istream &is)
    {
        uint8_t nextIndex = 0;
        std::array<uint8_t, 8> next{};
        is.read(reinterpret_cast<char *>(&nextIndex), 1);
        is.read(reinterpret_cast<char *>(next.data()), next.size());
        bool valid = nextIndex <= 7;
        for (uint8_t value : next)
            valid = valid && value <= 7;
        if (!is || !valid)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        index = nextIndex;
        registers = next;
    }
}
