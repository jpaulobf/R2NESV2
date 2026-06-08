#pragma once

#include <cstdint>
#include <array>
#include "Common/Common.h"
#include <iostream>

namespace R2NES::Core
{
    class VRAM
    {
    public:
        VRAM();
        ~VRAM();

        /* Limpa toda a memória das Name Tables. */
        void reset();

        uint8_t read(uint16_t addr, MirrorMode mode) const;
        void write(uint16_t addr, uint8_t data, MirrorMode mode);

        uint8_t readRaw(uint16_t addr) const { return vram[addr & 0x07FF]; }
        void writeRaw(uint16_t addr, uint8_t data) { vram[addr & 0x07FF] = data; }
        size_t getSize() const { return vram.size(); }

        // Serialização para Save / Load states
        void saveState(std::ostream &os);
        void loadState(std::istream &is);

    private:
        uint16_t getMirroredAddress(uint16_t addr, MirrorMode mode) const;

        std::array<uint8_t, 2048> vram;
    };
}