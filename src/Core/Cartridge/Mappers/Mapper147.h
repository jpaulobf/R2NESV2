#pragma once
#include "Core/Cartridge/Mappers/Mapper011.h"

namespace R2NES::Core
{
    // TC-U01 de 72 pinos: latch discreto, sem a proteção JV001 da placa de 60 pinos.
    class Mapper147 : public Mapper011
    {
    public:
        Mapper147(uint16_t prg, uint16_t chr, MirrorMode mirror);
        static bool supports(uint16_t prg, uint16_t chr, MirrorMode mirror);
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
    };
}
