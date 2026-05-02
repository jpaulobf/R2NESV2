#include "Core/Memory/VRAM/VRAM.h"
#include <algorithm>

namespace R2NES::Core
{
    VRAM::VRAM()
    {
        // Inicializa os 2KB de memória das Name Tables.
        // vram deve ser um std::array<uint8_t, 2048> ou similar no seu .h
        std::fill(vram.begin(), vram.end(), 0x00);
    }

    VRAM::~VRAM()
    {
    }

    uint8_t VRAM::read(uint16_t addr, MirrorMode mode) const
    {
        return vram[getMirroredAddress(addr, mode)];
    }

    void VRAM::write(uint16_t addr, uint8_t data, MirrorMode mode)
    {
        vram[getMirroredAddress(addr, mode)] = data;
    }

    uint16_t VRAM::getMirroredAddress(uint16_t addr, MirrorMode mode) const
    {
        // O espaço de Name Tables na PPU vai de 0x2000 a 0x2FFF (com espelhos até 0x3EFF).
        // Normalizamos para 0x0000 - 0x0FFF (os 4KB lógicos das 4 Name Tables).
        uint16_t maskedAddr = (addr - 0x2000) & 0x0FFF;

        switch (mode)
        {
        case MirrorMode::VERTICAL:
            // NT0 (0x2000) e NT2 (0x2800) apontam para os primeiros 1KB físicos.
            // NT1 (0x2400) e NT3 (0x2C00) apontam para os segundos 1KB físicos.
            return maskedAddr & 0x07FF;

        case MirrorMode::HORIZONTAL:
            // NT0 (0x2000) e NT1 (0x2400) apontam para os primeiros 1KB físicos.
            // NT2 (0x2800) e NT3 (0x2C00) apontam para os segundos 1KB físicos.
            if (maskedAddr < 0x0800)
                return maskedAddr & 0x03FF; // NT0 e NT1
            else
                return (maskedAddr & 0x03FF) + 0x0400; // NT2 e NT3

        case MirrorMode::ONESCREEN_LO:
            return maskedAddr & 0x03FF;

        case MirrorMode::ONESCREEN_HI:
            return (maskedAddr & 0x03FF) + 0x0400;

        default:
            // Fallback para Vertical (comum em muitos jogos)
            return maskedAddr & 0x07FF;
        }
    }
}