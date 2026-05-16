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
            return maskedAddr & 0x07FF;

        case MirrorMode::HORIZONTAL:
            // Ignora o bit 10 (A10) e usa o bit 11 (A11) para selecionar o banco de 1KB
            return (maskedAddr & 0x03FF) | ((maskedAddr & 0x0800) >> 1);

        case MirrorMode::ONESCREEN_LO:
            return maskedAddr & 0x03FF;

        case MirrorMode::ONESCREEN_HI:
            return (maskedAddr & 0x03FF) + 0x0400;

        case MirrorMode::FOUR_SCREEN:
            // Proteção contra overflow enquanto não houver 4KB reais
            return maskedAddr & 0x07FF; 

        default:
            // Fallback para Vertical (comum em muitos jogos)
            return maskedAddr & 0x07FF;
        }
    }
}