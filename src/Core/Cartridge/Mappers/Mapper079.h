#pragma once
#include "Core/Cartridge/Mappers/Mapper011.h"

namespace R2NES::Core
{
    // NINA-03/06 e SA-016: janelas e estados iguais aos bancos discretos do Mapper11.
    class Mapper079 : public Mapper011
    {
    public:
        Mapper079(uint16_t prg, uint16_t chr, MirrorMode mirror);
        static bool supports(uint16_t prg, uint16_t chr, MirrorMode mirror);
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
    };
}
