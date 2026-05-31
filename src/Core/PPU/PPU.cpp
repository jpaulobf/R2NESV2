#include "Core/PPU/PPU.h"
#include "Core/Bus/Bus.h"
#include "Core/Cartridge/Cartridge.h"
#include "Common/Common.h"
#include <algorithm>
#include <iostream>

namespace R2NES::Core
{
    // Paleta de cores padrão do NES (64 cores ARGB)
    // Esta tabela converte o índice de cor (0-63) da Palette RAM em um valor de cor real.
    static const uint32_t nesSystemPalette[64] = {
        0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088, 0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800, 0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00, 0xFF003260, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4, 0xFF8814B0, 0xFFA01468, 0xFFB03200, 0xFF8C4400, 0xFF546000, 0xFF207400, 0xFF087C00, 0xFF007A28, 0xFF006C90, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFECEEE8, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC, 0xFFE454EC, 0xFFEC58B4, 0xFFEC6A5C, 0xFFD48820, 0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C, 0xFF38B4CC, 0xFF3E3E3E, 0xFF000000, 0xFF000000,
        0xFFFCFCFC, 0xFFA4D2FC, 0xFFB8B8FC, 0xFFD8A8FC, 0xFFF8A4FC, 0xFFF8A8D8, 0xFFF8B4B4, 0xFFF0C090, 0xFFD8D470, 0xFFC4E470, 0xFFB0EC90, 0xFFA4ECAF, 0xFFA4E2FC, 0xFFB8B8B8, 0xFF000000, 0xFF000000};

    PPU::PPU()
    {
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000); // Inicializa com preto opaco
        std::fill(oamMemory.begin(), oamMemory.end(), 0xFF); // Inicializa fora da tela (Y=255)

        // Debug: Inicializa paletas com valores padrão para o Viewer funcionar sem ROM carregar paletas
        vramAddr = 0;
        tempAddr = 0;
        fineX = 0;

        scanline = -1;
        cycle = 0;
        sprite0HitDetectedThisScanline = false;
    }

    PPU::~PPU()
    {
    }

    void PPU::connectBus(Bus *bus)
    {
        this->bus = bus;
    }

    uint8_t PPU::cpuRead(uint16_t addr)
    {
        addr &= 0x0007;
        switch (addr)
        {
        case 0x0002: // PPUSTATUS ($2002)
        {
            // Retorna o status (vblank, sprite 0 hit, etc)
            uint8_t data = (ppuStatus & 0xE0) | (dataBuffer & 0x1F);

            // No NES real, apenas o bit de VBlank ($80) é limpo na leitura de $2002.
            // O bit de Sprite 0 Hit ($40) permanece setado até o pré-render scanline.
            ppuStatus &= ~0x80;

            // No NES real, $2005 e $2006 compartilham o mesmo latch de escrita (w)
            addressLatch = 0;
            return data;
        }

        case 0x0004: // OAMDATA ($2004)
            // Leitura da memória OAM baseada no oamAddr
            return oamMemory[oamAddr];

        case 0x0007: // PPUDATA ($2007)
        {
            // Leituras do PPUDATA são atrasadas por um buffer, exceto para Paletas
            uint8_t data = dataBuffer;
            dataBuffer = ppuRead(vramAddr & 0x3FFF);

            // Se estivermos lendo paletas, o dado é retornado imediatamente
            if ((vramAddr & 0x3FFF) >= 0x3F00)
            {
                // Paletas retornam dado imediato, mas o buffer é preenchido com o dado da VRAM "atrás" (nametable)
                data = ppuRead(vramAddr & 0x3FFF);
                dataBuffer = ppuRead((vramAddr & 0x3FFF) & 0x2FFF); // Espelhamento de VRAM abaixo das paletas
            }

            vramAddr += (ppuCtrl & 0x04) ? 32 : 1;
            return data;
        }
        }
        return 0x00;
    }

    void PPU::cpuWrite(uint16_t addr, uint8_t data)
    {
        addr &= 0x0007; // Mapeia o intervalo $2000-$3FFF para os 8 registradores básicos
        switch (addr)
        {
        case 0x0000: // PPUCTRL ($2000)
        {
            uint8_t oldNmiEnabled = ppuCtrl & 0x80;
            ppuCtrl = data;
            tempAddr = (tempAddr & 0xF3FF) | ((static_cast<uint16_t>(data) & 0x03) << 10);
            // Se habilitar NMI durante o VBlank, dispara imediatamente
            if (!oldNmiEnabled && (ppuCtrl & 0x80) && (ppuStatus & 0x80))
                nmi = true;
            break;
        }

        case 0x0001: // PPUMASK ($2001)
        {
            ppuMask = data;
            // Bits importantes para Sprite 0 Hit:
            // Bit 3: Background enable
            // Bit 4: Sprite enable
            break;
        }

        case 0x0003: // OAMADDR ($2003)
            oamAddr = data;
            break;

        case 0x0004: // OAMDATA ($2004)
            oamMemory[oamAddr] = data;
            oamAddr++;
            break;

        case 0x0005: // PPUSCROLL ($2005)
        {
            if (addressLatch == 0)
            {
                // Primeira escrita: Coarse X e Fine X
                fineX = data & 0x07;
                tempAddr = (tempAddr & 0xFFE0) | (data >> 3);
                addressLatch = 1;
            }
            else
            {
                // Segunda escrita: Coarse Y e Fine Y
                tempAddr = (tempAddr & 0x8C1F) | ((static_cast<uint16_t>(data) & 0x07) << 12) | ((static_cast<uint16_t>(data) & 0xF8) << 2);
                addressLatch = 0;
            }
            break;
        }

        case 0x0006: // PPUADDR ($2006)
            // Escrita dupla: primeiro MSB, depois LSB
            if (addressLatch == 0)
            {
                tempAddr = (tempAddr & 0x00FF) | ((static_cast<uint16_t>(data) & 0x3F) << 8);
                addressLatch = 1;
            }
            else
            {
                tempAddr = (tempAddr & 0xFF00) | data;
                vramAddr = tempAddr;
                addressLatch = 0;
            }
            break;

        case 0x0007: // PPUDATA ($2007)
            ppuWrite(vramAddr & 0x3FFF, data);
            vramAddr = (vramAddr + ((ppuCtrl & 0x04) ? 32 : 1)) & 0x7FFF;
            break;
        }
    }

    uint8_t PPU::ppuRead(uint16_t addr) const
    {
        uint8_t data = 0x00;
        addr &= 0x3FFF;

        if (bus && bus->ppuRead(addr, data))
        {
            return data;
        }

        // Se o Mapper não respondeu e o endereço está no range de Name Tables
        if (addr >= 0x2000 && addr <= 0x3EFF)
        {
            return vram.read(addr, bus ? bus->getMirrorMode() : MirrorMode::HORIZONTAL);
        }

        // Se não for do cartucho, verifica paletas
        if (addr >= 0x3F00 && addr <= 0x3FFF)
        {
            addr &= 0x001F;
            // Espelhamento de paletas: $3F10/$3F14/$3F18/$3F1C -> $3F00/$3F04/$3F08/$3F0C
            // (endereços sprite background) -> (endereços universal background)
            if ((addr & 0x0013) == 0x0010)
                addr &= 0x000F;
            return paletteTable[addr];
        }

        return 0x00;
    }

    void PPU::ppuWrite(uint16_t addr, uint8_t data)
    {
        addr &= 0x3FFF;

        if (bus && bus->ppuWrite(addr, data))
        {
            return;
        }

        if (addr >= 0x2000 && addr <= 0x3EFF)
        {
            vram.write(addr, data, bus ? bus->getMirrorMode() : MirrorMode::HORIZONTAL);
            return;
        }

        if (addr >= 0x3F00 && addr <= 0x3FFF)
        {
            addr &= 0x001F;
            // No NES, os endereços $3F10, $3F14, $3F18 e $3F1C são espelhos de
            // $3F00, $3F04, $3F08 e $3F0C respectivamente (cores de fundo).
            // Quando escrevemos em $3F00, também escrevemos em $3F10, $3F14, $3F18, $3F1C
            if ((addr & 0x0013) == 0x0010)
                addr &= 0x000F;
            paletteTable[addr] = data;
        }
    }

    std::vector<uint32_t> PPU::getPatternTablePixels(uint8_t patternTableIndex, uint8_t paletteIndex) const
    {
        // Uma Pattern Table tem 16x16 tiles. Cada tile tem 8x8 pixels.
        // Total: 128x128 pixels.
        std::vector<uint32_t> pixels(128 * 128);

        for (uint16_t tileY = 0; tileY < 16; tileY++)
        {
            for (uint16_t tileX = 0; tileX < 16; tileX++)
            {
                // Endereço base do tile: (índice da table * 4096) + (índice do tile * 16 bytes por tile)
                uint16_t offset = (patternTableIndex * 4096) + (tileY * 16 + tileX) * 16;

                for (uint16_t row = 0; row < 8; row++)
                {
                    // Cada linha do tile é composta por 2 bytes (2 planes)
                    uint8_t tileLSB = 0;
                    uint8_t tileMSB = 0;

                    if (bus)
                    {
                        bus->ppuRead(offset + row, tileLSB);
                        bus->ppuRead(offset + row + 8, tileMSB);
                    }

                    for (uint16_t col = 0; col < 8; col++)
                    {
                        // O bit 7 é o pixel mais à esquerda.
                        // Combinamos o bit do plane 0 (LSB) e plane 1 (MSB) para ter o índice da cor (0-3)
                        uint8_t pixelColorValue = ((tileLSB >> (7 - col)) & 0x01) | (((tileMSB >> (7 - col)) & 0x01) << 1);

                        // Resolve a cor final usando a paleta selecionada
                        // Endereço na Palette RAM: $3F00 + (paletteIndex * 4) + pixelColorValue
                        uint16_t paletteAddr = 0x3F00 + (paletteIndex * 4) + pixelColorValue;
                        uint8_t systemPaletteIndex = ppuRead(paletteAddr) & 0x3F;

                        // Escreve no buffer de pixels na posição correta da imagem 128x128
                        uint32_t pixelX = tileX * 8 + col;
                        uint32_t pixelY = tileY * 8 + row;
                        pixels[pixelY * 128 + pixelX] = nesSystemPalette[systemPaletteIndex];
                    }
                }
            }
        }

        return pixels;
    }

    void PPU::incrementScrollX()
    {
        if ((vramAddr & 0x001F) == 31)
        {
            vramAddr &= ~0x001F;
            vramAddr ^= 0x0400;
        }
        else
        {
            vramAddr++;
        }
    }

    void PPU::incrementScrollY()
    {
        if ((vramAddr & 0x7000) != 0x7000)
        {
            vramAddr += 0x1000;
        }
        else
        {
            vramAddr &= ~0x7000;
            uint16_t y = (vramAddr & 0x03E0) >> 5;
            if (y == 29)
            {
                y = 0;
                vramAddr ^= 0x0800;
            }
            else if (y == 31)
            {
                y = 0;
            }
            else
            {
                y++;
            }
            vramAddr = (vramAddr & ~0x03E0) | (y << 5);
        }
    }

    void PPU::transferAddressX()
    {
        vramAddr = (vramAddr & 0xFBE0) | (tempAddr & 0x041F);
    }

    void PPU::transferAddressY()
    {
        vramAddr = (vramAddr & 0x841F) | (tempAddr & 0x7BE0);
    }

    void PPU::clock()
    {
        // Lógica de atualização de Scroll baseada em ciclos
        bool renderingEnabled = (ppuMask & 0x08) || (ppuMask & 0x10);

        // Reset de flags de status deve ocorrer independente de renderingEnabled
        if (scanline == -1 && cycle == 1)
        {
            // Limpa flags de VBlank, Sprite 0 Hit e Overflow no início do pre-render
            ppuStatus &= ~0xE0; 
            sprite0HitDetectedThisScanline = false;
        }

        if (renderingEnabled)
        {
            // O pre-render scanline (-1) prepara o scroll para o próximo frame
            if (scanline == -1)
            {
                if (cycle >= 280 && cycle <= 304)
                    transferAddressY();
            }

            if (scanline >= -1 && scanline < 240)
            {
                if (cycle > 0 && cycle <= 256 && (cycle % 8 == 0))
                    incrementScrollX();
                if (cycle == 256)
                    incrementScrollY();
                if (cycle == 257)
                    transferAddressX();
            }
        }

        // No início de cada scanline visível (ciclo 0), avaliamos quais sprites serão desenhados.
        // No hardware real, isso acontece durante o scanline anterior, mas para fins de emulação,
        // fazer no ciclo 0 é eficiente e preciso o suficiente para a maioria dos casos.
        if (cycle == 0)
        {
            scanlineSpriteCount = 0;
            int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;

            if (renderingEnabled && scanline >= 0 && scanline < 240)
            {
                for (int i = 0; i < 64; i++)
                {
                    uint8_t spriteY = oamMemory[i * 4];
                    int diffY = scanline - (spriteY + 1);

                    if (diffY >= 0 && diffY < spriteHeight)
                    {
                        if (scanlineSpriteCount < 8)
                        {
                            scanlineSprites[scanlineSpriteCount++] = (uint8_t)i;
                        }
                        else
                        {
                            ppuStatus |= 0x20; // Sprite Overflow
                            if (unlimitedSprites)
                                scanlineSprites[scanlineSpriteCount++] = (uint8_t)i;
                            else
                                break;
                        }
                    }
                }
            }
        }

        // Só processamos renderização nos ciclos visíveis (0-255) e scanlines visíveis (0-239)
        if (scanline >= 0 && scanline < 240 && cycle >= 0 && cycle < 256)
        {
            uint8_t bgPixelColor = 0;
            uint8_t bgPaletteIndex = 0;

            // Renderiza background se habilitado (PPUMASK bit 3)
            // Nos primeiros 8 pixels (ciclos 0-7), verifica bit 1 (show background in leftmost 8 pixels)
            bool bgRenderingEnabled = (ppuMask & 0x08) != 0;
            if (cycle < 8 && !(ppuMask & 0x02))
                bgRenderingEnabled = false;

            if (bgRenderingEnabled)
            {
                // Cálculo do endereço de busca levando em conta o fineX (Scroll Fino Horizontal)
                // Se (ciclo atual + fineX) passar de 8, precisamos buscar dados do PRÓXIMO tile
                uint16_t currentV = vramAddr;
                uint8_t fX = (cycle + fineX) % 8;

                if ((cycle % 8) + fineX >= 8)
                {
                    // Incremento horizontal temporário para a busca do tile adjacente
                    if ((currentV & 0x001F) == 31)
                        currentV = (currentV & ~0x001F) ^ 0x0400;
                    else
                        currentV++;
                }

                uint16_t tileX = currentV & 0x001F;
                uint16_t tileY = (currentV & 0x03E0) >> 5;
                uint16_t ntBase = 0x2000 | (currentV & 0x0C00);
                uint8_t fY = (currentV & 0x7000) >> 12;

                uint8_t tileID = ppuRead(ntBase + tileY * 32 + tileX);

                uint16_t bgPtBase = (ppuCtrl & 0x10) ? 0x1000 : 0x0000;
                uint8_t bgLsb = ppuRead(bgPtBase + tileID * 16 + fY);
                uint8_t bgMsb = ppuRead(bgPtBase + tileID * 16 + fY + 8);

                bgPixelColor = ((bgLsb >> (7 - fX)) & 0x01) | (((bgMsb >> (7 - fX)) & 0x01) << 1);

                // 7. Busca a paleta na Attribute Table
                uint16_t attrAddr = ntBase + 0x3C0 + ((tileY / 4) * 8) + (tileX / 4); // A tabela de atributos começa em 0x3C0 dentro de cada nametable
                uint8_t attrByte = ppuRead(attrAddr);
                uint8_t paletteShift = ((tileY % 4) / 2 * 2 + (tileX % 4) / 2) * 2;
                bgPaletteIndex = (attrByte >> paletteShift) & 0x03;
            }

            // Resolve a cor do background
            uint16_t bgPaletteAddr = 0x3F00 + (bgPaletteIndex * 4) + bgPixelColor;
            if (bgPixelColor == 0)
                bgPaletteAddr = 0x3F00;
            frameBuffer[scanline * 256 + cycle] = nesSystemPalette[ppuRead(bgPaletteAddr) & 0x3F];

            // --- Renderização de Sprites (Otimizada para este ciclo) ---
            bool spriteRenderingEnabled = (ppuMask & 0x10) != 0;
            // Nos primeiros 8 pixels (ciclos 0-7), verifica bit 2 (show sprites in leftmost 8 pixels)
            if (cycle < 8 && !(ppuMask & 0x04))
                spriteRenderingEnabled = false;

            bool spritePixelDrawn = false;

            if (spriteRenderingEnabled)
            {
                for (int j = 0; j < scanlineSpriteCount; j++)
                {
                    uint8_t i = scanlineSprites[j];
                    uint8_t spriteY = oamMemory[i * 4];
                    int diffY = scanline - (spriteY + 1);
                    int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;

                    uint8_t spriteX = oamMemory[i * 4 + 3];
                    // diffX pode ser negativo (sprite ainda não começou) ou > 7 (sprite já terminou)
                    int diffX = cycle - spriteX;

                    // Debug: se Sprite 0 mudou de posição, avisa
                    if (i == 0 && (spriteY != lastSprite0Y || spriteX != lastSprite0X))
                    {
                        lastSprite0Y = spriteY;
                        lastSprite0X = spriteX;
                    }

                    // Se o ciclo atual está dentro da largura horizontal do sprite (0-7 pixels)
                    if (diffX >= 0 && diffX < 8)
                    {
                        uint8_t spriteID = oamMemory[i * 4 + 1];
                        uint8_t spriteAttrib = oamMemory[i * 4 + 2];

                        // Para sprites 8x8: spPtBase é controlado por ppuCtrl bit 3
                        // Para sprites 8x16: spPtBase é sempre 0x0000 e selecionado pelo bit 0 do spriteID
                        uint16_t spPtBase;
                        uint8_t pattern = spriteID;
                        uint8_t row = 0;

                        if (spriteHeight == 16)
                        {
                            // No modo 8x16, o bit 0 do tile seleciona a Pattern Table
                            spPtBase = (spriteID & 0x01) * 0x1000;
                            pattern = (spriteID & 0xFE);

                            if (spriteAttrib & 0x80) // Flip vertical
                            {
                                // Com flip: inverter a linha e selecionar padrão corretamente
                                int flippedY = 15 - diffY;
                                pattern = (spriteID & 0xFE) | ((flippedY >> 3) & 0x01);
                                row = flippedY & 0x07;
                            }
                            else // Sem flip vertical
                            {
                                // Metade superior (0-7): padrão com bit 0 = 0
                                // Metade inferior (8-15): padrão com bit 0 = 1
                                pattern = (spriteID & 0xFE) | ((diffY >> 3) & 0x01);
                                row = diffY & 0x07;
                            }
                        }
                        else
                        {
                            // Sprites 8x8: padrão é direto, spPtBase controlado por ppuCtrl bit 3
                            spPtBase = (ppuCtrl & 0x08) ? 0x1000 : 0x0000;
                            row = (spriteAttrib & 0x80) ? (7 - diffY) : diffY;
                        }

                        // Para flip horizontal: invertemos como acessamos os bits da pattern
                        // Sem flip: bit 7 (esquerda) a bit 0 (direita) = (7 - diffX)
                        // Com flip: bit 0 (esquerda) a bit 7 (direita) = diffX
                        uint8_t col = (spriteAttrib & 0x40) ? diffX : (7 - diffX);

                        uint8_t spLsb = ppuRead(spPtBase + pattern * 16 + row);
                        uint8_t spMsb = ppuRead(spPtBase + pattern * 16 + row + 8);
                        uint8_t spritePixelColor = ((spLsb >> col) & 0x01) | (((spMsb >> col) & 0x01) << 1);

                        if (spritePixelColor != 0) // Pixel não é transparente
                        {
                            // ========== SPRITE 0 HIT DETECTION ==========
                            bool bgHasPixel = (bgPixelColor != 0);
                            bool cycleInValidRange = (cycle >= 1 && cycle <= 254);
                            bool renderingEnabled = (ppuMask & 0x08) && (ppuMask & 0x10);

                            if (cycle < 8 && (!(ppuMask & 0x02) || !(ppuMask & 0x04)))
                                cycleInValidRange = false;

                            if (i == 0 && bgHasPixel && renderingEnabled && !sprite0HitDetectedThisScanline && cycleInValidRange)
                            {
                                ppuStatus |= 0x40;
                                sprite0HitDetectedThisScanline = true;
                            }

                            bool priority = (spriteAttrib & 0x20) == 0;
                            if (priority || bgPixelColor == 0)
                            {
                                uint8_t spritePalette = (spriteAttrib & 0x03) + 4;
                                uint16_t palAddr = 0x3F00 + (spritePalette * 4) + spritePixelColor;

                                // Debug: Pintar o Sprite 0 de Lilás (Magenta) para facilitar o rastreio do Sprite 0 Hit
                                if (i == 0 && usedDebugColors)
                                    frameBuffer[scanline * 256 + cycle] = 0xFFFF00FF;
                                else
                                    frameBuffer[scanline * 256 + cycle] = nesSystemPalette[ppuRead(palAddr) & 0x3F];

                                spritePixelDrawn = true;
                            }

                            // Se desenhamos um pixel de sprite opaco, ele oculta os sprites de menor prioridade
                            break;
                        }
                    }
                }
            }

            // ZAPPER
            // Verifica uma área de 5x5 em volta da mira para facilitar o acerto (emula a lente da pistola)
            if (cycle >= zapperX - 2 && cycle <= zapperX + 2 &&
                scanline >= zapperY - 2 && scanline <= zapperY + 2)
            {
                // Pegamos a cor final que foi parar no framebuffer para este pixel
                uint32_t finalPixelColor = frameBuffer[scanline * 256 + cycle];
                // Se o brilho for alto (ex: branco do flash do pato), detecta luz.
                // No seu palette, branco é 0xFFFCFCFC. Vamos checar se o canal R é alto.
                if (((finalPixelColor >> 16) & 0xFF) > 0xEE)
                    zapperLightDetected = true;
            }
        }

        cycle++;
        if (cycle >= 341)
        {
            cycle = 0;
            scanline++;

            // Reseta o flag de Sprite 0 Hit para o próximo scanline
            sprite0HitDetectedThisScanline = false;

            if (scanline == 241)
            {
                // Início do Vertical Blank
                ppuStatus |= 0x80;
                frameComplete = true;

                if (ppuCtrl & 0x80)
                    nmi = true;
            }
            else if (scanline >= 261)
            {
                scanline = -1;
                zapperLightDetected = false;
                frameCounter++;
            }
        }

        // Implementação do Odd Frame Cycle Skip
        if (scanline == -1 && cycle == 339 && renderingEnabled && (frameCounter % 2 != 0))
        {
            cycle = 340; 
        }
    }

    void PPU::reset()
    {
        ppuCtrl = 0x00;
        ppuMask = 0x00;
        ppuStatus = 0x00; // O ideal é resetar para algum estado, mas bit 7 costuma manter
        oamAddr = 0x00;
        addressLatch = 0;
        vramAddr = 0;
        tempAddr = 0;
        fineX = 0;
        sprite0HitDetectedThisScanline = false;
        scanline = -1;
        cycle = 0;
        scanlineSpriteCount = 0;
        frameCounter = 0;
        nmi = false;

        // Limpa o buffer de imagem para preto ao resetar/descarregar
        std::fill(oamMemory.begin(), oamMemory.end(), 0xFF); // Move sprites para fora da tela
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000);
    }
}