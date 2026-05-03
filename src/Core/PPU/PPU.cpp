#include "Core/PPU/PPU.h"
#include "Core/Bus/Bus.h"
#include "Core/Cartridge/Cartridge.h"
#include "Common/Common.h"
#include <algorithm>

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

        // Debug: Inicializa paletas com valores padrão para o Viewer funcionar sem ROM carregar paletas
        paletteTable[0] = 0x0F; // Background universal (Preto)
        paletteTable[1] = 0x01; // Azul
        paletteTable[2] = 0x11; // Azul claro
        paletteTable[3] = 0x21; // Azul muito claro
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
            // Retorna o status (vblank, sprite 0 hit, etc) e limpa a flag de vblank
            uint8_t data = (ppuStatus & 0xE0) | (dataBuffer & 0x1F);
            ppuStatus &= ~0x80; // Limpa bit de VBlank após leitura
            addressLatch = 0;   // Reseta o latch de escrita dupla ($2005/$2006)
            return data;
        }

        case 0x0007: // PPUDATA ($2007)
        {
            // Leituras do PPUDATA são atrasadas por um buffer, exceto para Paletas
            uint8_t data = dataBuffer;
            dataBuffer = ppuRead(ppuAddress);

            // Se estivermos lendo paletas, o dado é retornado imediatamente
            if (ppuAddress >= 0x3F00) data = dataBuffer;

            ppuAddress += 1; // TODO: Incrementar por 32 se PPUCTRL bit 2 estiver setado
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
            ppuCtrl = data;
            break;

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
        // Lógica de temporização simplificada para permitir que o jogo rode
        cycle++;
        if (cycle >= 341)
        {
            cycle = 0;
            scanline++;
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
                ppuStatus &= ~0x80; // Limpa flag de VBlank no fim do pre-render
            }
        }
    }

    void PPU::reset()
    {
        ppuCtrl = 0x00;
        ppuStatus = 0x00;
        addressLatch = 0;
        ppuAddress = 0;
    }
}