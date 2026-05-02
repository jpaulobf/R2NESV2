#pragma once

#include <cstdint>
#include <array>
#include "Common/Common.h"

namespace R2NES::Core
{
    class VRAM
    {
    public:
        VRAM();
        ~VRAM();

        uint8_t read(uint16_t addr, MirrorMode mode) const;
        void write(uint16_t addr, uint8_t data, MirrorMode mode);

    private:
        uint16_t getMirroredAddress(uint16_t addr, MirrorMode mode) const;

        std::array<uint8_t, 2048> vram;
    };
}