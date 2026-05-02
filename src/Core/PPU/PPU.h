#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include "Common/Common.h"
#include "Core/Memory/VRAM/VRAM.h"

namespace R2NES::Core
{
    class Cartridge;

    class PPU
    {
    public:
        PPU();
        ~PPU();

        // Comunicação com o barramento da CPU ($2000 - $2007)
        uint8_t cpuRead(uint16_t addr);
        void cpuWrite(uint16_t addr, uint8_t data);

        // Comunicação com o barramento interno da PPU ($0000 - $3FFF)
        uint8_t ppuRead(uint16_t addr);
        void ppuWrite(uint16_t addr, uint8_t data);

        void clock();
        void reset();
        void setCartridge(const std::shared_ptr<Cartridge> &cartridge);
        bool isFrameComplete() const { return frameComplete; }
        void clearFrameComplete() { frameComplete = false; }

        // Retorna um buffer de pixels ARGB para um Pattern Table específico
        // patternTableIndex: 0 para Pattern Table 0 ($0000-$0FFF), 1 para Pattern Table 1 ($1000-$1FFF)
        std::vector<uint32_t> getPatternTablePixels(uint8_t patternTableIndex, uint8_t paletteIndex = 0) const;

        // Retorna o ponteiro para o buffer de pixels (256x240)
        const uint32_t* getFrameBuffer() const { return frameBuffer.data(); }

    private:
        std::shared_ptr<Cartridge> cart;
        VRAM vram; // Name Tables gerenciadas pela classe que você criou

        // Paletas internas (32 bytes)
        // 0x3F00-0x3F0F: Paletas de fundo
        // 0x3F10-0x3F1F: Paletas de sprites
        std::array<uint8_t, 32> paletteTable;

        // Buffer de saída de imagem (RGBA8888)
        std::array<uint32_t, 256 * 240> frameBuffer;

        // Registradores e buffers internos
        uint8_t addressLatch = 0x00;
        uint8_t dataBuffer = 0x00;
        uint16_t ppuAddress = 0x0000;

        // Estado da renderização
        int16_t scanline = 0;
        int16_t cycle = 0;

        bool frameComplete = true;
    };
}