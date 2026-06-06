#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include "Common/Common.h"
#include "Core/Memory/VRAM/VRAM.h"
#include <iostream>

namespace R2NES::Core
{
    class Bus;

    class PPU
    {
    public:
        PPU();
        ~PPU();

        // Comunicação com o barramento da CPU ($2000 - $2007)
        uint8_t cpuRead(uint16_t addr);
        void cpuWrite(uint16_t addr, uint8_t data);
        bool nmi = false;

        // Comunicação com o barramento interno da PPU ($0000 - $3FFF)
        uint8_t ppuRead(uint16_t addr) const;
        void ppuWrite(uint16_t addr, uint8_t data);

        void clock();
        void reset();
        void connectBus(Bus *bus);
        bool isFrameComplete() const { return frameComplete; }

        // Zapper Interface
        void setZapperPos(int x, int y)
        {
            zapperX = x;
            zapperY = y;
        }
        bool getZapperLightSense() const { return zapperLightDetected; }

        void clearFrameComplete() { frameComplete = false; }
        void setUnlimitedSprites(bool enabled) { unlimitedSprites = enabled; }

        // Serialização para Save / Load states
        void saveState(std::ostream &os);
        void loadState(std::istream &is);

        // Retorna um buffer de pixels ARGB para um Pattern Table específico
        // patternTableIndex: 0 para Pattern Table 0 ($0000-$0FFF), 1 para Pattern Table 1 ($1000-$1FFF)
        std::vector<uint32_t> getPatternTablePixels(uint8_t patternTableIndex, uint8_t paletteIndex = 0) const;

        // Retorna o ponteiro para o buffer de pixels (256x240)
        const uint32_t *getFrameBuffer() const { return frameBuffer.data(); }

    private:
        Bus *bus = nullptr;
        VRAM vram; // Name Tables gerenciadas pela classe que você criou

        // Paletas internas (32 bytes)
        // 0x3F00-0x3F0F: Paletas de fundo
        // 0x3F10-0x3F1F: Paletas de sprites
        std::array<uint8_t, 32> paletteTable;

        // Memória OAM (Object Attribute Memory) - 64 sprites * 4 bytes
        std::array<uint8_t, 256> oamMemory;

        // Buffer de saída de imagem (RGBA8888)
        std::array<uint32_t, 256 * 240> frameBuffer;

        // Zapper State
        int zapperX = -1, zapperY = -1;
        bool zapperLightDetected = false;

        // Registradores e buffers internos
        uint8_t ppuCtrl = 0x00; // PPUCTRL ($2000)
        uint8_t ppuMask = 0x00; // PPUMASK ($2001) - Controla renderização
        uint8_t oamAddr = 0x00;
        uint8_t ppuStatus = 0x00;
        uint8_t addressLatch = 0x00;
        uint8_t dataBuffer = 0x00;

        // Registradores Internos (Loopy's Registers)
        uint16_t vramAddr = 0x0000; // v: Endereço de VRAM atual (15 bits)
        uint16_t tempAddr = 0x0000; // t: Endereço de VRAM temporário (15 bits)
        uint8_t fineX = 0x00;       // x: Scroll fino horizontal (3 bits)

        // Sprite 0 Hit - Tracking
        // No NES real, o Sprite 0 Hit é detectado apenas uma vez por scanline
        // Depois de ser detectado, o bit permanece setado até o pré-render scanline
        bool sprite0HitDetectedThisScanline = false;
        uint32_t lastSprite0Y = 0xFF;
        uint32_t lastSprite0X = 0xFF;

        // Estado da renderização
        int16_t scanline = 0;
        int16_t cycle = 0;

        // Cache de sprites para o scanline atual (máximo 64 para o modo ilimitado)
        std::array<uint8_t, 64> scanlineSprites;
        int scanlineSpriteCount = 0;

        bool frameComplete = false;
        bool unlimitedSprites = false;
        uint32_t frameCounter = 0;
        bool usedDebugColors = false;

        // Métodos auxiliares para Loopy Registers
        void incrementScrollX();
        void incrementScrollY();
        void transferAddressX();
        void transferAddressY();
    };
}