#include "Core/Cartridge/Mappers/Mapper147.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper147::supports(uint16_t prg, uint16_t chr, MirrorMode mirror)
    {
        return Mapper011::supports(prg, chr, mirror) && prg >= 2 && prg <= 4 && chr >= 1;
    }
    Mapper147::Mapper147(uint16_t prg, uint16_t chr, MirrorMode mirror) : Mapper011(prg, chr, mirror)
    {
        if (!supports(prg, chr, mirror))
            throw std::invalid_argument("Unsupported Mapper 147 72-pin configuration");
    }
    bool Mapper147::cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock)
    {
        if ((addr & 0x4103) != 0x4102)
            return false;
        // PCCCCP..: bit 2 escolhe PRG de 32 KiB; bits 3-6 escolhem CHR de 8 KiB.
        // A placa de 72 pinos não conecta o segundo bit PRG (bit 7).
        return Mapper011::cpuMapWrite(0x8000, mapped, uint8_t(((data >> 3) & 15) << 4 | ((data >> 2) & 1)), clock);
    }
}
