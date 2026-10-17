#include "Core/Cartridge/Mappers/Mapper079.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper079::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        return Mapper011::supports(prg, chr, mirror) && prg >= 2 && prg <= 4 && chr >= 1 && chr <= 8;
    }
    Mapper079::Mapper079(uint16_t prg, uint16_t chr, MirrorMode mirror) : Mapper011(prg, chr, mirror)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 79 configuration");
    }
    bool Mapper079::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        // SA-016 não possui o chaveamento CNROM extra de dumps mal identificados.
        if ((addr & 0xE100) != 0x4100)
            return false;
        return Mapper011::cpuMapWrite(0x8000, mapped, uint8_t(((data & 7) << 4) | ((data >> 3) & 1)), clock);
    }
}
