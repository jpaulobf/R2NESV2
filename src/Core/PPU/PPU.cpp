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
        0xFFFCFCFC, 0xFFA4D2FC, 0xFFB8B8FC, 0xFFD8A8FC, 0xFFF8A4FC, 0xFFF8A8D8, 0xFFF8B4B4, 0xFFF0C090, 0xFFD8D470, 0xFFC4E470, 0xFFB0EC90, 0xFFA4ECAF, 0xFFA4E2FC, 0xFFB8B8B8, 0xFF000000, 0xFF000000
    };

    PPU::PPU()
    {
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000); // Inicializa com preto opaco
        std::fill(oamMemory.begin(), oamMemory.end(), 0x00);

        // Debug: Inicializa paletas com valores padrão para o Viewer funcionar sem ROM carregar paletas
        paletteTable[0] = 0x0F; // Background universal (Preto)
        paletteTable[1] = 0x01; // Azul
        paletteTable[2] = 0x11; // Azul claro
        paletteTable[3] = 0x21; // Azul muito claro

        sprite0HitDetectedThisScanline = false;
    }

    PPU::~PPU()
    {
    }

    void PPU::connectBus(Bus* bus)
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
            dataBuffer = ppuRead(ppuAddress);

            // Se estivermos lendo paletas, o dado é retornado imediatamente
            if (ppuAddress >= 0x3F00) data = dataBuffer;

            ppuAddress += (ppuCtrl & 0x04) ? 32 : 1;
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
            // Se habilitar NMI durante o VBlank, dispara imediatamente
            if (!oldNmiEnabled && (ppuCtrl & 0x80) && (ppuStatus & 0x80)) nmi = true;
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
                // Primeira escrita: X scroll
                scrollX = data;
                addressLatch = 1;
            }
            else
            {
                // Segunda escrita: Y scroll
                scrollY = data;
                addressLatch = 0;
            }
            break;
        }

        case 0x0006: // PPUADDR ($2006)
            // Escrita dupla: primeiro MSB, depois LSB
            if (addressLatch == 0)
            {
                ppuAddress = (ppuAddress & 0x00FF) | (static_cast<uint16_t>(data & 0x3F) << 8);
                addressLatch = 1;
            }
            else
            {
                ppuAddress = (ppuAddress & 0xFF00) | data;
                addressLatch = 0;
            }
            break;

        case 0x0007: // PPUDATA ($2007)
            ppuWrite(ppuAddress, data);
            ppuAddress += (ppuCtrl & 0x04) ? 32 : 1;
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
            if ((addr & 0x0013) == 0x0010) addr &= 0x000F;
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
            if ((addr & 0x0013) == 0x0010) addr &= 0x000F;
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
                    
                    if (bus) {
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

    void PPU::clock() 
    {
        // Só processamos renderização nos ciclos visíveis (0-255) e scanlines visíveis (0-239)
        if (scanline >= 0 && scanline < 240 && cycle >= 0 && cycle < 256)
        {
            uint8_t bgPixelColor = 0;
            uint8_t bgPaletteIndex = 0;

            // 1. Aplica o scroll e calcula a posição no espaço de renderização virtual (256x240 * N nametables)
            uint16_t renderX = (cycle + scrollX) % 512;  // Permite wrapping horizontal
            uint16_t renderY = (scanline + scrollY) % 480; // Permite wrapping vertical (240 * 2)

            // 2. Determina a posição do tile e do pixel dentro do tile
            uint16_t tileX = (renderX / 8) % 32;     // Coarse X (0-31)
            uint16_t tileY = (renderY / 8) % 32;     // Coarse Y (0-31) (Nametables são 32x32 tiles em memória)
            uint16_t fineX = renderX % 8;            // Pixel fino horizontal (0-7)
            uint16_t fineY = renderY % 8;            // Pixel fino vertical (0-7)

            // 3. Determina qual nametable estamos usando a partir do ppuAddress (registrador 'v' interno da PPU).
            // Os bits 10 e 11 do ppuAddress (VRAM address) selecionam a nametable.
            // ppuAddress é o endereço VRAM atual que a PPU está processando.
            uint8_t ntIndexFromPPUAddress = (ppuAddress >> 10) & 0x03;
            uint16_t ntBase = 0x2000 + (ntIndexFromPPUAddress * 0x400);

            // 4. Busca o ID do Tile na Name Table
            uint8_t tileID = ppuRead(ntBase + tileY * 32 + tileX);

            // 5. Busca os bits do pixel na Pattern Table
            uint16_t bgPtBase = (ppuCtrl & 0x10) ? 0x1000 : 0x0000;
            uint8_t bgLsb = ppuRead(bgPtBase + tileID * 16 + fineY);
            uint8_t bgMsb = ppuRead(bgPtBase + tileID * 16 + fineY + 8);

            bgPixelColor = ((bgLsb >> (7 - fineX)) & 0x01) | (((bgMsb >> (7 - fineX)) & 0x01) << 1);

            // 6. Busca a paleta na Attribute Table
            uint16_t attrAddr = ntBase + 0x3C0 + ((tileY / 4) * 8) + (tileX / 4); // A tabela de atributos começa em 0x3C0 dentro de cada nametable
            uint8_t attrByte = ppuRead(attrAddr);
            uint8_t paletteShift = ((tileY % 4) / 2 * 2 + (tileX % 4) / 2) * 2;
            bgPaletteIndex = (attrByte >> paletteShift) & 0x03;

            // Resolve a cor do background
            uint16_t bgPaletteAddr = 0x3F00 + (bgPaletteIndex * 4) + bgPixelColor;
            if (bgPixelColor == 0) bgPaletteAddr = 0x3F00;
            frameBuffer[scanline * 256 + cycle] = nesSystemPalette[ppuRead(bgPaletteAddr) & 0x3F];

            // --- Renderização de Sprites (Otimizada para este ciclo) ---
            bool spritePixelDrawn = false;
            for (int i = 0; i < 64; i++)
            {
                uint8_t spriteY = oamMemory[i * 4];
                int diffY = scanline - (spriteY + 1);
                int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;

                if (diffY >= 0 && diffY < spriteHeight)
                {
                    uint8_t spriteX = oamMemory[i * 4 + 3];
                    int diffX = cycle - spriteX;

                    // Debug: se Sprite 0 mudou de posição, avisa
                    if (i == 0 && (spriteY != lastSprite0Y || spriteX != lastSprite0X))
                    {
                        lastSprite0Y = spriteY;
                        lastSprite0X = spriteX;
                    }

                    // Se o ciclo atual da PPU está dentro da largura horizontal do sprite
                    if (diffX >= 0 && diffX < 8)
                    {
                        uint8_t spriteID = oamMemory[i * 4 + 1];
                        uint8_t spriteAttrib = oamMemory[i * 4 + 2];
                        uint16_t spPtBase = (ppuCtrl & 0x08) ? 0x1000 : 0x0000;
                        
                        uint8_t row = (spriteAttrib & 0x80) ? (spriteHeight - 1 - diffY) : diffY;
                        uint8_t col = (spriteAttrib & 0x40) ? diffX : (7 - diffX);

                        uint8_t spLsb = ppuRead(spPtBase + spriteID * 16 + row);
                        uint8_t spMsb = ppuRead(spPtBase + spriteID * 16 + row + 8);
                        uint8_t spritePixelColor = ((spLsb >> col) & 0x01) | (((spMsb >> col) & 0x01) << 1);

                        if (spritePixelColor != 0) // Pixel não é transparente
                        {
                            // ========== SPRITE 0 HIT DETECTION ==========
                            // Hardware real: Sprite 0 Hit requer AMBAS condições:
                            // 1. Sprite 0 com pixel opaco
                            // 2. Background com pixel opaco
                            // 3. Renderização de background E sprites habilitada no PPUMASK
                            // 4. Não pode ocorrer no ciclo 255 (quirk do hardware)
                            bool bgHasPixel = (bgPixelColor != 0); // Verifica se o pixel de fundo não é transparente
                            bool cycleInValidRange = (cycle >= 1 && cycle <= 254); // Sprite 0 hit pode ocorrer do ciclo 1 ao 254
                            bool renderingEnabled = (ppuMask & 0x08) && (ppuMask & 0x10);

                            // Se o clipping de 8px estiver ativo, o hit não ocorre nessa área
                            if (cycle < 8 && (!(ppuMask & 0x02) || !(ppuMask & 0x04))) cycleInValidRange = false;

                            if (i == 0 && bgHasPixel && renderingEnabled && !sprite0HitDetectedThisScanline && cycleInValidRange)
                            {
                                ppuStatus |= 0x40;
                                sprite0HitDetectedThisScanline = true;
                            }

                            if (i == 0 && spritePixelColor != 0 && bgPixelColor == 0 && false)
                            {
                                std::cout << "--- Sprite 0 Hit Falhou ---" << std::endl;
                                std::cout << "Scanline: " << scanline << ", Cycle: " << cycle << std::endl;
                                std::cout << "SpriteX: " << (int)spriteX << ", SpriteY: " << (int)spriteY << std::endl;
                                std::cout << "scrollX: " << (int)scrollX << ", scrollY: " << (int)scrollY << std::endl;
                                std::cout << "ppuCtrl: 0x" << std::hex << (int)ppuCtrl << ", ppuMask: 0x" << (int)ppuMask << std::dec << std::endl;
                                std::cout << "renderX: " << renderX << ", renderY: " << renderY << std::endl;
                                std::cout << "tileX: " << tileX << ", tileY: " << tileY << std::endl;
                                std::cout << "fineX: " << fineX << ", fineY: " << fineY << std::endl;
                                std::cout << "ntBase: 0x" << std::hex << ntBase << std::dec << std::endl;
                                std::cout << "tileID: " << (int)tileID << std::endl;
                                std::cout << "bgLsb: 0x" << std::hex << (int)bgLsb << ", bgMsb: 0x" << (int)bgMsb << std::dec << std::endl;
                                std::cout << "bgPixelColor: " << (int)bgPixelColor << std::endl;
                                std::cout << "---------------------------" << std::endl;
                            }

                            bool priority = (spriteAttrib & 0x20) == 0;
                            if (priority || bgPixelColor == 0)
                            {
                                uint8_t spritePalette = (spriteAttrib & 0x03) + 4;
                                uint16_t palAddr = 0x3F00 + (spritePalette * 4) + spritePixelColor;
                                frameBuffer[scanline * 256 + cycle] = nesSystemPalette[ppuRead(palAddr) & 0x3F];
                                spritePixelDrawn = true;
                            }
                            
                            // Se desenhamos um pixel de sprite opaco, ele oculta os sprites de menor prioridade (índice maior)
                            break;
                        }
                    }
                }
            }
        }

        cycle++;
        if (cycle >= 341)
        {
            cycle = 0;
            scanline++;

            // Reseta o flag de Sprite 0 Hit para o próximo scanline
            // (mantém o bit setado em ppuStatus até o pré-render scanline)
            sprite0HitDetectedThisScanline = false;

            if (scanline == 241) 
            {
                // Início do Vertical Blank
                ppuStatus |= 0x80; // Seta flag de VBlank
                frameComplete = true;
                
                // Se NMIs estiverem habilitados no PPUCTRL (bit 7), sinaliza para a CPU
                if (ppuCtrl & 0x80)
                    nmi = true;
            }
            else if (scanline >= 261)
            {
                scanline = -1;
                ppuStatus &= ~0xE0; // Limpa VBlank (7), Sprite 0 Hit (6) e Sprite Overflow (5)
            }
        }
    }

    void PPU::reset()
    {
        ppuCtrl = 0x00;
        ppuMask = 0x00;
        ppuStatus = 0x00; // O ideal é resetar para algum estado, mas bit 7 costuma manter
        oamAddr = 0x00;
        addressLatch = 0;
        ppuAddress = 0;
        scrollX = 0x00;
        scrollY = 0x00;
        sprite0HitDetectedThisScanline = false;
        scanline = 0;
        cycle = 0;
    }
}