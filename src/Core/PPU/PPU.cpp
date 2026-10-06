#include "Core/PPU/PPU.h"
#include "Core/Bus/Bus.h"
#include "Core/Cartridge/Cartridge.h"
#include "Common/Common.h"
#include <algorithm>
#include <iostream>
#include <utility>

namespace R2NES::Core
{
    // Paleta de cores padrão do NES (64 cores ARGB)
    // Esta tabela converte o índice de cor (0-63) da Palette RAM em um valor de cor real.
    static const uint32_t nesSystemPalette[64] = {
        0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088, 0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
        0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00, 0xFF003260, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4, 0xFF8814B0, 0xFFA01468, 0xFFB03200, 0xFF8C4400,
        0xFF546000, 0xFF207400, 0xFF087C00, 0xFF007A28, 0xFF006C90, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFECEEE8, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC, 0xFFE454EC, 0xFFEC58B4, 0xFFEC6A5C, 0xFFD48820,
        0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C, 0xFF38B4CC, 0xFF3E3E3E, 0xFF000000, 0xFF000000,
        0xFFFCFCFC, 0xFFA4D2FC, 0xFFB8B8FC, 0xFFD8A8FC, 0xFFF8A4FC, 0xFFF8A8D8, 0xFFF8B4B4, 0xFFF0C090,
        0xFFD8D470, 0xFFC4E470, 0xFFB0EC90, 0xFFA4ECAF, 0xFFA4E2FC, 0xFFB8B8B8, 0xFF000000, 0xFF000000};

    namespace
    {
        uint8_t reverseSpriteBits(uint8_t value)
        {
            value = static_cast<uint8_t>(((value & 0xF0) >> 4) | ((value & 0x0F) << 4));
            value = static_cast<uint8_t>(((value & 0xCC) >> 2) | ((value & 0x33) << 2));
            return static_cast<uint8_t>(((value & 0xAA) >> 1) | ((value & 0x55) << 1));
        }

        bool isBattletoadsSpriteZeroWait(const Bus *bus)
        {
            if (!bus || !bus->cpu || !bus->cart || bus->cart->getRomHash() != "279710DC" ||
                bus->cpu->a != 0x40)
                return false;

            uint16_t loopAddress;
            if (bus->cpu->pc == 0x8641)
                loopAddress = 0x863E;
            else if (bus->cpu->pc == 0x8631)
                loopAddress = 0x862E;
            else
                return false;

            constexpr uint8_t loopSignature[] = {0x2C, 0x02, 0x20, 0xF0, 0xFB};
            for (size_t offset = 0; offset < sizeof(loopSignature); ++offset)
            {
                uint8_t value = 0;
                if (!bus->cart->cpuRead(loopAddress + offset, value) || value != loopSignature[offset])
                    return false;
            }
            return true;
        }

        bool isBattletoadsDoubleDragonSpriteZeroWait(const Bus *bus)
        {
            if (!bus || !bus->cpu || !bus->cart || bus->cart->getRomHash() != "CEB65B06" ||
                bus->cpu->a != 0x40)
                return false;

            uint16_t loopAddress;
            if (bus->cpu->pc == 0x8193)
                loopAddress = 0x8190;
            else if (bus->cpu->pc == 0x8183)
                loopAddress = 0x8180;
            else
                return false;

            constexpr uint8_t loopSignature[] = {0x2C, 0x02, 0x20, 0xF0, 0xFB};
            for (size_t offset = 0; offset < sizeof(loopSignature); ++offset)
            {
                uint8_t value = 0;
                if (!bus->cart->cpuRead(loopAddress + offset, value) || value != loopSignature[offset])
                    return false;
            }
            return true;
        }
    }

    static const uint32_t PALETTE_SMOOTH[64] = {
        0xFF6A6A6A, 0xFF001E8C, 0xFF0610A0, 0xFF2A009B, 0xFF4E007A, 0xFF5B0047, 0xFF570012, 0xFF450D00,
        0xFF292200, 0xFF093300, 0xFF003D00, 0xFF003D17, 0xFF003754, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFB5B5B5, 0xFF004ADC, 0xFF2B36FA, 0xFF6320EF, 0xFF9514C3, 0xFFA91681, 0xFFA4253A, 0xFF8F3D00,
        0xFF695600, 0xFF376E00, 0xFF147B00, 0xFF007A41, 0xFF007197, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFFFFFFF, 0xFF3C96FF, 0xFF6C81FF, 0xFFA76BFF, 0xFFDD5DFF, 0xFFF85CD2, 0xFFF66D81, 0xFFE1873A,
        0xFFBAA206, 0xFF87BA00, 0xFF60C823, 0xFF45C874, 0xFF40BFD1, 0xFF454545, 0xFF000000, 0xFF000000,
        0xFFFFFFFF, 0xFFA9D4FF, 0xFFBDCCFF, 0xFFD5C3FF, 0xFFEBBDFF, 0xFFF7BDEE, 0xFFF7C4CE, 0xFFEFCEB1,
        0xFFDED99C, 0xFFC9E39C, 0xFFB9E9AB, 0xFFAEE9CC, 0xFFACE5F2, 0xFFAEAEAE, 0xFF000000, 0xFF000000};

    static const uint32_t PALETTE_NESTOPIA[64] = {
        0xFF7C7C7C, 0xFF0000FC, 0xFF0000BC, 0xFF4428BC, 0xFF940084, 0xFFA80020, 0xFFA81000, 0xFF881400,
        0xFF503000, 0xFF007800, 0xFF006800, 0xFF005800, 0xFF004058, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFBCBCBC, 0xFF0078F8, 0xFF0058F8, 0xFF6844FC, 0xFFD800CC, 0xFFE40058, 0xFFF83800, 0xFFE45C10,
        0xFFAC7C00, 0xFF00B800, 0xFF00A800, 0xFF00A844, 0xFF008888, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFF878F8, 0xFF3CBCFC, 0xFF68A0FC, 0xFFB488FC, 0xFFF878F8, 0xFFFD78B4, 0xFFF88444, 0xFFF89800,
        0xFFE4D410, 0xFF58D854, 0xFF58F898, 0xFF00E8D8, 0xFF787878, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFFCFCFC, 0xFFA4E4FC, 0xFFB8B8F8, 0xFFD8B8F8, 0xFFF8B8F8, 0xFFF8A4C0, 0xFFF0D0B0, 0xFFFCE0A8,
        0xFFF8D878, 0xFFD8F878, 0xFFB8F8B8, 0xFFB8F8D8, 0xFF00FCFC, 0xFFF8D8F8, 0xFF000000, 0xFF000000};

    static const uint32_t PALETTE_WAVEBEAM[64] = {
        0xFF525252, 0xFF00009C, 0xFF0000B5, 0xFF3100A5, 0xFF6B007B, 0xFF8B0039, 0xFF830000, 0xFF5A0C00,
        0xFF292200, 0xFF003500, 0xFF003D00, 0xFF003518, 0xFF002A52, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF9C9C9C, 0xFF0042F7, 0xFF0021F7, 0xFF5A00F7, 0xFFA500D6, 0xFFD60073, 0xFFCE1000, 0xFF9C3100,
        0xFF5A4A00, 0xFF006300, 0xFF007300, 0xFF006B42, 0xFF005A9C, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFECEEEC, 0xFF319CF7, 0xFF527BF7, 0xFF9C5AF7, 0xFFED42F7, 0xFFF731BD, 0xFFF74A5A, 0xFFDE6B00,
        0xFF9C8B00, 0xFF31A500, 0xFF00B521, 0xFF00AD73, 0xFF009CDE, 0xFF313131, 0xFF000000, 0xFF000000,
        0xFFECEEEC, 0xFFADDEF7, 0xFFBDCEF7, 0xFFD6BDF7, 0xFFF7BDF7, 0xFFF7BDDE, 0xFFF7C6BD, 0xFFF7CE9C,
        0xFFDEDE9C, 0xFFBDE79C, 0xFF9CEFBD, 0xFF9CEEDE, 0xFF9CE7F7, 0xFFBDBDBD, 0xFF000000, 0xFF000000};

    static const uint32_t PALETTE_CYBER_NEON[64] = {
        0xFF6A6A6A, 0xFF8C1E00, 0xFF0610A0, 0xFF9B002A, 0xFF7A004E, 0xFF47005B, 0xFF120057, 0xFF000D45,
        0xFF002229, 0xFF003309, 0xFF003D00, 0xFF173D00, 0xFF543700, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFB5B5B5, 0xFFDC4A00, 0xFF2B36FA, 0xFFEF2063, 0xFFC31495, 0xFF8116A9, 0xFF3B25A4, 0xFF003D8F,
        0xFF005669, 0xFF006E37, 0xFF007B14, 0xFF417A00, 0xFF977100, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFFFFFFF, 0xFFFF963C, 0xFF6C81FF, 0xFFFF6BA7, 0xFFFF5DDD, 0xFFD25CF8, 0xFF816DF6, 0xFF3A87E1,
        0xFF06A2BA, 0xFF00BA87, 0xFF23C860, 0xFF74C845, 0xD1BF40FF, 0xFF454545, 0xFF000000, 0xFF000000,
        0xFFFFFFFF, 0xFFD4A9FF, 0xFFBDCCFF, 0xFFC3D5FF, 0xFFBDEBFF, 0xFFBDF7EE, 0xFFC4F7CE, 0xCEEFB1FF,
        0xFF9CD9DE, 0xFF9CE3C9, 0xABE9B9FF, 0xCCE9AEFF, 0xF2E5ACFF, 0xFFAEAEAE, 0xFF000000, 0xFF000000};

    // Inicialização da variável estática (feita uma única vez no escopo global)
    const uint32_t *PPU::currentPalette = nesSystemPalette;

    PPU::PPU()
    {
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000); // Inicializa com preto opaco
        std::fill(oamMemory.begin(), oamMemory.end(), 0xFF);           // Inicializa fora da tela (Y=255)
        secondaryOam.fill(0xFF);

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

    const uint32_t *PPU::getSystemPalette()
    {
        return currentPalette;
    }

    void PPU::setSystemPalette(PaletteType type)
    {
        switch (type)
        {
        case PaletteType::DEFAULT:
            currentPalette = nesSystemPalette;
            break;
        case PaletteType::SMOOTH:
            currentPalette = PALETTE_SMOOTH;
            break;
        case PaletteType::NESTOPIA:
            currentPalette = PALETTE_NESTOPIA;
            break;
        case PaletteType::WAVEBEAM:
            currentPalette = PALETTE_WAVEBEAM;
            break;
        case PaletteType::NEON:
            currentPalette = PALETTE_CYBER_NEON;
            break;
        default:
            currentPalette = nesSystemPalette;
            break;
        }
    }

    void PPU::connectBus(Bus *bus)
    {
        this->bus = bus;
    }

    uint8_t PPU::cpuRead(uint16_t addr, bool readOnly)
    {
        addr &= 0x0007;
        switch (addr)
        {
        case 0x0002: // PPUSTATUS ($2002)
        {
            // Retorna o status (vblank, sprite 0 hit, etc)
            uint8_t data = (ppuStatus & 0xE0) | (dataBuffer & 0x1F);

            const bool mapper7RasterHitHack = !readOnly && scanline == 30 && cycle >= 255 && cycle < 341 &&
                                              ppuCtrl == 0x10 && ppuMask == 0x18 &&
                                              oamMemory[0] == 0x1C && oamMemory[3] == 0xFE &&
                                              isBattletoadsSpriteZeroWait(bus);
            const int sprite0Height = (ppuCtrl & 0x20) ? 16 : 8;
            const int sprite0FirstScanline = static_cast<int>(oamMemory[0]) + 1;
            const int sprite0X = oamMemory[3];
            const int sprite0LastVisibleX = std::min(sprite0X + 7, 254);
            constexpr int sprite0HitEarlyTolerance = 3;
            const int sprite0HitWindowStartCycle = std::max(sprite0X + 2,
                                                            sprite0LastVisibleX + 2 - sprite0HitEarlyTolerance);
            const bool sprite0RasterPassed = sprite0X <= 254 &&
                                             scanline >= sprite0FirstScanline &&
                                             scanline < sprite0FirstScanline + sprite0Height &&
                                             cycle >= sprite0HitWindowStartCycle && cycle <= 256;
            const bool mapper7DoubleDragonWaitHack = !readOnly && (ppuMask & 0x18) == 0x18 && sprite0RasterPassed &&
                                                     isBattletoadsDoubleDragonSpriteZeroWait(bus);
            if ((data & 0x40) == 0 && (mapper7RasterHitHack || mapper7DoubleDragonWaitHack))
            {
                ppuStatus |= 0x40;
                data |= 0x40;
                static bool loggedHack = false;
                if (!loggedHack)
                {
                    std::cerr << "PPU: Battletoads Sprite 0 wait-loop hack enabled" << std::endl;
                    loggedHack = true;
                }
            }

            // No NES real, apenas o bit de VBlank ($80) é limpo na leitura de $2002.
            // O bit de Sprite 0 Hit ($40) permanece setado até o pré-render scanline.
            if (!readOnly)
            {
                ppuStatus &= ~0x80;
                // No NES real, $2005 e $2006 compartilham o mesmo latch de escrita (w)
                addressLatch = 0;
            }
            return data;
        }

        case 0x0004: // OAMDATA ($2004)
            // Durante a renderização, a CPU observa o barramento interno usado
            // pela limpeza, avaliação e busca dos sprites.
            return isRenderingOamAccess() ? oamDataBusLatch : oamMemory[oamAddr];

        case 0x0007: // PPUDATA ($2007)
        {
            const uint16_t addr = vramAddr & 0x3FFF;
            if (readOnly)
                return addr >= 0x3F00 ? readPalette(addr) : dataBuffer;

            uint8_t data;
            if (addr >= 0x3F00)
            {
                // A paleta é interna ao PPU; só o endereço espelhado da
                // nametable acessa a memória externa e atualiza o A12 do MMC3.
                if (bus)
                    bus->ppuAddressUpdated(addr);
                data = readPalette(addr);
                dataBuffer = ppuFetch(addr & 0x2FFF);
            }
            else
            {
                data = dataBuffer;
                dataBuffer = ppuFetch(addr);
            }

            incrementDataAddress();
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
            if (isRenderingActive())
            {
                // A PPU ocupa a OAM durante a renderização. A escrita é
                // ignorada e o endereço avança para a próxima entrada n.
                oamAddr = static_cast<uint8_t>((oamAddr & 0x03) | ((oamAddr + 4) & 0xFC));
            }
            else
            {
                oamMemory[oamAddr] = data;
                oamDataBusLatch = data;
                oamAddr++;
            }
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
            incrementDataAddress();
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

    uint8_t PPU::ppuFetch(uint16_t addr) const
    {
        addr &= 0x3FFF;
        if (bus)
            bus->ppuAddressUpdated(addr);
        return ppuRead(addr);
    }

    uint8_t PPU::ppuFetchSprite(uint16_t addr) const
    {
        addr &= 0x3FFF;
        if (bus)
            bus->ppuAddressUpdated(addr);
        return ppuReadSprite(addr);
    }

    uint8_t PPU::readPalette(uint16_t addr) const
    {
        addr &= 0x001F;
        if ((addr & 0x0013) == 0x0010)
            addr &= 0x000F;
        return paletteTable[addr];
    }

    bool PPU::isRenderingOamAccess() const
    {
        return isRenderingActive() && cycle >= 1 && cycle <= 320;
    }

    bool PPU::isRenderingActive() const
    {
        const bool renderingScanline = scanline == -1 || (scanline >= 0 && scanline < 240);
        return renderingScanline && (ppuMask & 0x18) != 0;
    }

    void PPU::incrementDataAddress()
    {
        if (isRenderingActive())
        {
            // Durante a renderização, o acesso a $2007 aciona os dois
            // incrementos do endereço de scroll, independentemente de PPUCTRL.
            incrementScrollX();
            incrementScrollY();
            vramAddr &= 0x7FFF;
            return;
        }

        vramAddr = (vramAddr + ((ppuCtrl & 0x04) ? 32 : 1)) & 0x7FFF;
    }

    uint8_t PPU::ppuReadSprite(uint16_t addr) const
    {
        uint8_t data = 0x00;
        addr &= 0x3FFF;

        if (bus && bus->ppuReadSprite(addr, data))
            return data;

        return ppuRead(addr);
    }

    void PPU::ppuWrite(uint16_t addr, uint8_t data)
    {
        addr &= 0x3FFF;

        if (bus)
            bus->ppuAddressUpdated(addr);

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
                        uint8_t systemPaletteIndex = readPalette(paletteAddr) & 0x3F;

                        // Escreve no buffer de pixels na posição correta da imagem 128x128
                        uint32_t pixelX = tileX * 8 + col;
                        uint32_t pixelY = tileY * 8 + row;
                        pixels[pixelY * 128 + pixelX] = currentPalette[systemPaletteIndex];
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

    void PPU::loadBackgroundShifters()
    {
        bgShifterPatternLow = (bgShifterPatternLow & 0xFF00) | bgNextTileLsb;
        bgShifterPatternHigh = (bgShifterPatternHigh & 0xFF00) | bgNextTileMsb;
        bgShifterAttrLow = (bgShifterAttrLow & 0xFF00) | ((bgNextTileAttr & 0b01) ? 0xFF : 0x00);
        bgShifterAttrHigh = (bgShifterAttrHigh & 0xFF00) | ((bgNextTileAttr & 0b10) ? 0xFF : 0x00);
    }

    void PPU::updateShifters()
    {
        if (ppuMask & 0x08) // Background enabled
        {
            bgShifterPatternLow <<= 1;
            bgShifterPatternHigh <<= 1;
            bgShifterAttrLow <<= 1;
            bgShifterAttrHigh <<= 1;
        }
    }

    void PPU::clock()
    {
        // Lógica de atualização de Scroll baseada em ciclos
        bool renderingEnabled = (ppuMask & 0x08) || (ppuMask & 0x10);

        // O VBlank começa no dot 1 da scanline 241. A borda do status e o NMI
        // precisam ocorrer no mesmo ponto do ciclo da PPU.
        if (scanline == 241 && cycle == 1)
        {
            ppuStatus |= 0x80;
            frameComplete = true;

            if (ppuCtrl & 0x80)
                nmi = true;
        }

        // Reset de flags de status deve ocorrer independente de renderingEnabled
        if (scanline == -1 && cycle == 1)
        {
            // Limpa flags de VBlank, Sprite 0 Hit e Overflow no início do pre-render
            ppuStatus &= 0x1F; // Limpa os 3 bits superiores (7, 6, 5)
            sprite0HitDetectedThisScanline = false;
        }

        if (renderingEnabled)
        {
            if (scanline >= -1 && scanline < 240)
            {
                if (scanline == -1 && cycle == 1)
                {
                    bgShifterPatternLow = 0;
                    bgShifterPatternHigh = 0;
                    bgShifterAttrLow = 0;
                    bgShifterAttrHigh = 0;
                }

                if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336))
                {
                    switch ((cycle - 1) % 8)
                    {
                    case 0:
                        loadBackgroundShifters();
                        bgNextTileId = ppuFetch(0x2000 | (vramAddr & 0x0FFF));
                        break;
                    case 2:
                        bgNextTileAttr = ppuFetch(0x23C0 | (vramAddr & 0x0C00) | ((vramAddr >> 4) & 0x38) | ((vramAddr >> 2) & 0x07));
                        if (vramAddr & 0x0040)
                            bgNextTileAttr >>= 4;
                        if (vramAddr & 0x0002)
                            bgNextTileAttr >>= 2;
                        bgNextTileAttr &= 0x03;
                        break;
                    case 4:
                        bgNextTileLsb = ppuFetch(((ppuCtrl & 0x10) ? 0x1000 : 0x0000) + ((uint16_t)bgNextTileId << 4) + ((vramAddr >> 12) & 0x07));
                        break;
                    case 6:
                        bgNextTileMsb = ppuFetch(((ppuCtrl & 0x10) ? 0x1000 : 0x0000) + ((uint16_t)bgNextTileId << 4) + ((vramAddr >> 12) & 0x07) + 8);
                        break;
                    case 7:
                        incrementScrollX();
                        break;
                    }
                }

                if (cycle == 256)
                {
                    incrementScrollY();
                }

                if (cycle == 257)
                {
                    loadBackgroundShifters();
                    transferAddressX();
                }

                if (cycle == 337 || cycle == 339)
                {
                    if (cycle == 337)
                        loadBackgroundShifters();
                    bgNextTileId = ppuFetch(0x2000 | (vramAddr & 0x0FFF));
                }

                // O pre-render scanline (-1) prepara o scroll para o próximo frame
                if (scanline == -1 && cycle >= 280 && cycle <= 304)
                {
                    transferAddressY();
                }
            }
        }

        // A avaliação da OAM prepara os sprites da próxima scanline. O hardware
        // limpa a OAM secundária nos dots 1-64 e avalia um byte por vez nos
        // dots 65-256. A opção Unlimited Sprites mantém o caminho de depuração.
        if (renderingEnabled && scanline >= -1 && scanline < 240)
        {
            if (scanline >= 0 && cycle == 1)
            {
                nextSprites.fill(SpriteFetchUnit{});
                nextScanlineSpriteCount = 0;
                spriteEvaluationIndex = 0;
                spriteEvaluationStartIndex = 0;
                spriteEvaluationByteIndex = 0;
                spriteEvaluationBytesCopied = 0;
                spriteEvaluationLatch = 0xFF;
                oamDataBusLatch = 0xFF;
                secondaryOamAddress = 0;
                spriteEvaluationComplete = false;
            }

            if (scanline >= 0 && cycle == 65)
            {
                // A avaliação começa no endereço OAMADDR observado neste dot.
                // Com um endereço desalinhado, o byte apontado é tratado como
                // Y e os bytes seguintes são agrupados como uma entrada de OAM.
                spriteEvaluationIndex = unlimitedSprites ? 0 : static_cast<uint8_t>(oamAddr >> 2);
                spriteEvaluationStartIndex = spriteEvaluationIndex;
                spriteEvaluationByteIndex = unlimitedSprites ? 0 : static_cast<uint8_t>(oamAddr & 0x03);
                spriteEvaluationBytesCopied = 0;
            }

            if (scanline >= 0 && cycle >= 1 && cycle <= 64)
            {
                oamDataBusLatch = 0xFF;
                if (cycle >= 2 && (cycle % 2) == 0)
                    secondaryOam[(cycle / 2) - 1] = 0xFF;
            }
            else if (scanline >= 0 && cycle >= 65 && cycle <= 256)
            {
                const int targetScanline = scanline + 1;
                const int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;

                if (unlimitedSprites)
                {
                    if (((cycle - 65) % 3) == 0 && spriteEvaluationIndex < 64)
                    {
                        const uint8_t index = spriteEvaluationIndex++;
                        const uint8_t spriteY = oamMemory[index * 4];
                        const int row = targetScanline - (static_cast<int>(spriteY) + 1);

                        if (row >= 0 && row < spriteHeight)
                        {
                            SpriteFetchUnit &sprite = nextSprites[nextScanlineSpriteCount++];
                            sprite.oamIndex = index;
                            sprite.y = spriteY;
                            sprite.tile = oamMemory[index * 4 + 1];
                            sprite.attributes = oamMemory[index * 4 + 2];
                            sprite.x = oamMemory[index * 4 + 3];
                            sprite.row = static_cast<uint8_t>(row);

                            if (nextScanlineSpriteCount <= 8)
                            {
                                const size_t secondaryIndex = (nextScanlineSpriteCount - 1) * 4;
                                secondaryOam[secondaryIndex] = sprite.y;
                                secondaryOam[secondaryIndex + 1] = sprite.tile;
                                secondaryOam[secondaryIndex + 2] = sprite.attributes;
                                secondaryOam[secondaryIndex + 3] = sprite.x;
                            }
                        }
                    }
                }
                else if ((cycle & 1) != 0)
                {
                    // Odd dots read primary OAM at OAM[n][m].
                    if (spriteEvaluationIndex < 64)
                    {
                        const size_t oamIndex = static_cast<size_t>(spriteEvaluationIndex) * 4 +
                                                spriteEvaluationByteIndex;
                        spriteEvaluationLatch = oamMemory[oamIndex];
                        oamDataBusLatch = spriteEvaluationLatch;
                    }
                    else
                    {
                        spriteEvaluationLatch = 0xFF;
                        oamDataBusLatch = spriteEvaluationLatch;
                    }
                }
                else
                {
                    oamDataBusLatch = static_cast<size_t>(secondaryOamAddress) < secondaryOam.size()
                                          ? spriteEvaluationLatch
                                          : secondaryOam[0];

                    if (!spriteEvaluationComplete)
                    {
                        const int row = targetScanline - (static_cast<int>(spriteEvaluationLatch) + 1);

                        if (static_cast<size_t>(secondaryOamAddress) < secondaryOam.size())
                        {
                            if (spriteEvaluationBytesCopied == 0)
                            {
                                if (row >= 0 && row < spriteHeight)
                                {
                                    secondaryOam[secondaryOamAddress] = spriteEvaluationLatch;
                                    SpriteFetchUnit &sprite = nextSprites[secondaryOamAddress / 4];
                                    // O endereço inicial de avaliação define a
                                    // identidade lógica do sprite 0. Se essa
                                    // entrada não estiver em faixa, a próxima
                                    // não assume essa identidade.
                                    sprite.oamIndex = spriteEvaluationIndex == spriteEvaluationStartIndex
                                                          ? uint8_t{0}
                                                          : spriteEvaluationIndex;
                                    sprite.y = spriteEvaluationLatch;
                                    sprite.row = static_cast<uint8_t>(row);
                                    ++secondaryOamAddress;
                                    spriteEvaluationBytesCopied = 1;
                                    spriteEvaluationByteIndex = (spriteEvaluationByteIndex + 1) & 0x03;
                                    if (spriteEvaluationByteIndex == 0)
                                        ++spriteEvaluationIndex;
                                }
                                else
                                {
                                    ++spriteEvaluationIndex;
                                    if (spriteEvaluationIndex >= 64)
                                        spriteEvaluationComplete = true;
                                }
                            }
                            else
                            {
                                secondaryOam[secondaryOamAddress] = spriteEvaluationLatch;
                                SpriteFetchUnit &sprite = nextSprites[secondaryOamAddress / 4];
                                switch (spriteEvaluationBytesCopied)
                                {
                                case 1:
                                    sprite.tile = spriteEvaluationLatch;
                                    break;
                                case 2:
                                    sprite.attributes = spriteEvaluationLatch;
                                    break;
                                case 3:
                                    sprite.x = spriteEvaluationLatch;
                                    break;
                                }

                                ++secondaryOamAddress;
                                ++spriteEvaluationBytesCopied;
                                spriteEvaluationByteIndex = (spriteEvaluationByteIndex + 1) & 0x03;
                                if (spriteEvaluationByteIndex == 0)
                                    ++spriteEvaluationIndex;

                                if (spriteEvaluationBytesCopied == 4)
                                {
                                    ++nextScanlineSpriteCount;
                                    spriteEvaluationBytesCopied = 0;
                                    if (spriteEvaluationIndex >= 64)
                                        spriteEvaluationComplete = true;
                                }
                            }
                        }
                        else
                        {
                            // Após oito sprites, o bug do 2C02 percorre OAM em
                            // diagonal: um Y fora de faixa incrementa n e m;
                            // um Y em faixa também avança m, mas carrega para n
                            // somente quando m passa de 3.
                            if (row >= 0 && row < spriteHeight)
                            {
                                ppuStatus |= 0x20;
                                ++spriteEvaluationByteIndex;
                                if (spriteEvaluationByteIndex == 4)
                                {
                                    spriteEvaluationByteIndex = 0;
                                    ++spriteEvaluationIndex;
                                }
                            }
                            else
                            {
                                ++spriteEvaluationIndex;
                                spriteEvaluationByteIndex = (spriteEvaluationByteIndex + 1) & 0x03;
                            }

                            if (spriteEvaluationIndex >= 64)
                                spriteEvaluationComplete = true;
                        }
                    }
                }
            }
            else if (cycle >= 257 && cycle <= 320)
            {
                // Durante a busca dos sprites, a PPU força OAMADDR a zero.
                oamAddr = 0;

                const int slot = (cycle - 257) / 8;
                const int phase = (cycle - 257) % 8;
                const int secondaryByte = phase < 4 ? phase : 3;
                oamDataBusLatch = secondaryOam[slot * 4 + secondaryByte];

                if (phase == 0 || phase == 2)
                    ppuFetch(0x2000 | (vramAddr & 0x0FFF)); // Busca fictícia de nametable
                else if (phase == 3)
                    nextSprites[slot].xCounter = nextSprites[slot].x;
                else if (phase == 4)
                {
                    SpriteFetchUnit &sprite = nextSprites[slot];
                    const int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;
                    const int patternRow = (sprite.attributes & 0x80) ? spriteHeight - 1 - sprite.row : sprite.row;
                    if (spriteHeight == 16)
                    {
                        const uint16_t table = (sprite.tile & 0x01) ? 0x1000 : 0x0000;
                        const uint8_t tile = static_cast<uint8_t>((sprite.tile & 0xFE) + (patternRow >> 3));
                        sprite.patternAddress = table + (static_cast<uint16_t>(tile) << 4) + (patternRow & 0x07);
                    }
                    else
                    {
                        const uint16_t table = (ppuCtrl & 0x08) ? 0x1000 : 0x0000;
                        sprite.patternAddress = table + (static_cast<uint16_t>(sprite.tile) << 4) + patternRow;
                    }
                    sprite.patternLow = ppuFetchSprite(sprite.patternAddress);
                }
                else if (phase == 6)
                {
                    nextSprites[slot].patternHigh = ppuFetchSprite(nextSprites[slot].patternAddress + 8);
                    if (nextSprites[slot].attributes & 0x40)
                    {
                        nextSprites[slot].patternLow = reverseSpriteBits(nextSprites[slot].patternLow);
                        nextSprites[slot].patternHigh = reverseSpriteBits(nextSprites[slot].patternHigh);
                    }
                }
            }
        }

        if (cycle == 0)
        {
            std::swap(currentSprites, nextSprites);
            scanlineSpriteCount = nextScanlineSpriteCount;
            nextScanlineSpriteCount = 0;

            if (renderingEnabled && scanline >= 0 && scanline < 240 && bus)
                bus->ppuScanlineStart();
        }

        // Só processamos renderização nos ciclos visíveis (1-256) e scanlines visíveis (0-239)
        if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256)
        {
            // Cada unidade de sprite mantém um contador X e dois shifters de
            // padrão. A amostra do dot é obtida antes de avançar os shifters.
            std::array<uint8_t, 8> hardwareSpritePixels{};
            if (renderingEnabled)
            {
                for (int i = 0; i < std::min(scanlineSpriteCount, 8); ++i)
                {
                    SpriteFetchUnit &sprite = currentSprites[i];
                    if (sprite.xCounter > 0)
                    {
                        --sprite.xCounter;
                    }
                    else
                    {
                        hardwareSpritePixels[i] = static_cast<uint8_t>(
                            ((sprite.patternLow >> 7) & 0x01) |
                            (((sprite.patternHigh >> 7) & 0x01) << 1));
                        sprite.patternLow <<= 1;
                        sprite.patternHigh <<= 1;
                    }
                }
            }

            uint8_t bgPixelColor = 0;
            uint8_t bgPaletteIndex = 0;

            // Renderiza background se habilitado (PPUMASK bit 3)
            // Nos primeiros 8 pixels (ciclos 1-8), verifica bit 1 (show background in leftmost 8 pixels)
            bool bgShouldRender = (ppuMask & 0x08) != 0;
            if (cycle <= 8 && !(ppuMask & 0x02))
                bgShouldRender = false;

            if (bgShouldRender)
            {
                uint16_t bitMux = 0x8000 >> fineX;

                uint8_t p0_pixel = (bgShifterPatternLow & bitMux) > 0;
                uint8_t p1_pixel = (bgShifterPatternHigh & bitMux) > 0;
                bgPixelColor = (p1_pixel << 1) | p0_pixel;

                uint8_t bg_pal0 = (bgShifterAttrLow & bitMux) > 0;
                uint8_t bg_pal1 = (bgShifterAttrHigh & bitMux) > 0;
                bgPaletteIndex = (bg_pal1 << 1) | bg_pal0;
            }

            // Resolve a cor do background
            uint16_t bgPaletteAddr = 0x3F00 + (bgPaletteIndex * 4) + bgPixelColor;
            if (bgPixelColor == 0)
                bgPaletteAddr = 0x3F00;

            // Apenas desenha no framebuffer se a renderização de tiles estiver habilitada
            if (tilesEnabled)
            {
                frameBuffer[scanline * 256 + (cycle - 1)] = currentPalette[readPalette(bgPaletteAddr) & 0x3F];
            }
            else
            {
                // Se a renderização de tiles estiver desabilitada, preenchemos o fundo com a cor universal
                // para evitar o efeito de "rastro" dos sprites. A cor universal está em $3F00.
                frameBuffer[scanline * 256 + (cycle - 1)] = currentPalette[readPalette(0x3F00) & 0x3F];
            }

            // --- Renderização de Sprites (Otimizada para este ciclo) ---
            bool spriteShouldRender = (ppuMask & 0x10) != 0;
            // Nos primeiros 8 pixels (ciclos 1-8), verifica bit 2 (show sprites in leftmost 8 pixels)
            if (cycle <= 8 && !(ppuMask & 0x04))
                spriteShouldRender = false;

            if (spriteShouldRender)
            {
                const int spriteCount = unlimitedSprites ? scanlineSpriteCount : std::min(scanlineSpriteCount, 8);
                for (int j = 0; j < spriteCount; j++)
                {
                    const SpriteFetchUnit &sprite = currentSprites[j];
                    const uint8_t i = sprite.oamIndex;
                    if (i >= 64)
                        continue;

                    const uint8_t spriteY = sprite.y;
                    const uint8_t spriteX = sprite.x;

                    // Debug: se Sprite 0 mudou de posição, avisa
                    if (i == 0 && (spriteY != lastSprite0Y || spriteX != lastSprite0X))
                    {
                        lastSprite0Y = spriteY;
                        lastSprite0X = spriteX;
                    }

                    uint8_t spritePixelColor = 0;
                    const uint8_t spriteAttrib = sprite.attributes;

                    if (!unlimitedSprites)
                    {
                        spritePixelColor = hardwareSpritePixels[j];
                    }
                    else
                    {
                        const int diffY = scanline - (static_cast<int>(spriteY) + 1);
                        const int spriteHeight = (ppuCtrl & 0x20) ? 16 : 8;
                        const int diffX = (cycle - 1) - spriteX;

                        // O modo ilimitado é uma opção de depuração e preserva
                        // a composição expandida usada antes dos shifters reais.
                        if (diffX < 0 || diffX >= 8)
                            continue;

                        const uint8_t col = (spriteAttrib & 0x40) ? diffX : (7 - diffX);
                        uint8_t spLsb = sprite.patternLow;
                        uint8_t spMsb = sprite.patternHigh;
                        const int patternRow = (spriteAttrib & 0x80) ? spriteHeight - 1 - diffY : diffY;
                        uint16_t patternAddress;
                        if (spriteHeight == 16)
                        {
                            const uint16_t table = (sprite.tile & 0x01) ? 0x1000 : 0x0000;
                            const uint8_t tile = static_cast<uint8_t>((sprite.tile & 0xFE) + (patternRow >> 3));
                            patternAddress = table + (static_cast<uint16_t>(tile) << 4) + (patternRow & 0x07);
                        }
                        else
                        {
                            const uint16_t table = (ppuCtrl & 0x08) ? 0x1000 : 0x0000;
                            patternAddress = table + (static_cast<uint16_t>(sprite.tile) << 4) + patternRow;
                        }
                        spLsb = ppuReadSprite(patternAddress);
                        spMsb = ppuReadSprite(patternAddress + 8);
                        spritePixelColor = static_cast<uint8_t>(((spLsb >> col) & 0x01) |
                                                                 (((spMsb >> col) & 0x01) << 1));
                    }

                    if (spritePixelColor == 0)
                        continue;

                    const bool bgHasPixel = (bgPixelColor != 0);
                    bool cycleInValidRange = ((cycle - 1) >= 0 && (cycle - 1) <= 254);
                    const bool bothLayersEnabled = (ppuMask & 0x18) == 0x18;

                    if ((cycle - 1) < 8 && (!(ppuMask & 0x02) || !(ppuMask & 0x04)))
                        cycleInValidRange = false;

                    if (i == 0 && bgHasPixel && bothLayersEnabled && !sprite0HitDetectedThisScanline && cycleInValidRange)
                    {
                        ppuStatus |= 0x40;
                        sprite0HitDetectedThisScanline = true;
                    }

                    const bool priority = (spriteAttrib & 0x20) == 0;
                    if (priority || bgPixelColor == 0)
                    {
                        const uint8_t spritePalette = (spriteAttrib & 0x03) + 4;
                        const uint16_t palAddr = 0x3F00 + (spritePalette * 4) + spritePixelColor;

                        if (spritesEnabled)
                        {
                            if (i == 0 && usedDebugColors)
                                frameBuffer[scanline * 256 + (cycle - 1)] = 0xFFFF00FF;
                            else
                                frameBuffer[scanline * 256 + (cycle - 1)] = currentPalette[readPalette(palAddr) & 0x3F];
                        }
                    }

                    break;
                }
            }

            // ZAPPER
            // Verifica uma área de 5x5 em volta da mira para facilitar o acerto (emula a lente da pistola)
            if ((cycle - 1) >= zapperX - 2 && (cycle - 1) <= zapperX + 2 &&
                scanline >= zapperY - 2 && scanline <= zapperY + 2)
            {
                // Pegamos a cor final que foi parar no framebuffer para este pixel
                uint32_t finalPixelColor = frameBuffer[scanline * 256 + (cycle - 1)];
                // Se o brilho for alto (ex: branco do flash do pato), detecta luz.
                // No seu palette, branco é 0xFFFCFCFC. Vamos checar se o canal R é alto.
                if (((finalPixelColor >> 16) & 0xFF) > 0xEE)
                    zapperLightDetected = true;
            }
        }

        if (renderingEnabled)
        {
            if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336))
            {
                updateShifters();
            }
        }

        // Em quadros ímpares com rendering ativo, o hardware pula o dot 340
        // do pré-render: depois de processar o dot 339, vai direto ao próximo
        // quadro (scanline 0, dot 0).
        if (scanline == -1 && cycle == 339 && renderingEnabled && (frameCounter % 2 != 0))
        {
            cycle = 0;
            scanline = 0;
            return;
        }

        cycle++;
        if (cycle >= 341)
        {
            cycle = 0;
            scanline++;

            // Reseta o flag de Sprite 0 Hit para o próximo scanline
            sprite0HitDetectedThisScanline = false;

            if (scanline >= 261)
            {
                scanline = -1;
                zapperLightDetected = false;
                frameCounter++;
            }
        }
    }

    void PPU::reset()
    {
        vram.reset();
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);

        ppuCtrl = 0x00;
        ppuMask = 0x00;
        ppuStatus = 0x00; // O ideal é resetar para algum estado, mas bit 7 costuma manter
        oamAddr = 0x00;
        oamDataBusLatch = 0xFF;
        addressLatch = 0;
        vramAddr = 0;
        tempAddr = 0;
        fineX = 0;
        sprite0HitDetectedThisScanline = false;
        scanline = -1;
        cycle = 0;
        scanlineSpriteCount = 0;
        nextScanlineSpriteCount = 0;
        spriteEvaluationIndex = 0;
        spriteEvaluationStartIndex = 0;
        spriteEvaluationByteIndex = 0;
        spriteEvaluationBytesCopied = 0;
        spriteEvaluationLatch = 0xFF;
        secondaryOamAddress = 0;
        spriteEvaluationComplete = false;
        currentSprites.fill(SpriteFetchUnit{});
        nextSprites.fill(SpriteFetchUnit{});
        secondaryOam.fill(0xFF);
        frameCounter = 0;
        frameComplete = false;
        nmi = false;
        zapperLightDetected = false;

        bgNextTileId = 0x00;
        bgNextTileAttr = 0x00;
        bgNextTileLsb = 0x00;
        bgNextTileMsb = 0x00;
        bgShifterPatternLow = 0x0000;
        bgShifterPatternHigh = 0x0000;
        bgShifterAttrLow = 0x0000;
        bgShifterAttrHigh = 0x0000;

        // Limpa o buffer de imagem para preto ao resetar/descarregar
        std::fill(oamMemory.begin(), oamMemory.end(), 0xFF); // Move sprites para fora da tela
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000);
    }

    void PPU::saveState(std::ostream &os)
    {
        vram.saveState(os);
        os.write(reinterpret_cast<const char *>(&ppuCtrl), sizeof(ppuCtrl));
        os.write(reinterpret_cast<const char *>(&ppuMask), sizeof(ppuMask));
        os.write(reinterpret_cast<const char *>(&ppuStatus), sizeof(ppuStatus));
        os.write(reinterpret_cast<const char *>(&oamAddr), sizeof(oamAddr));
        os.write(reinterpret_cast<const char *>(&oamDataBusLatch), sizeof(oamDataBusLatch));
        os.write(reinterpret_cast<const char *>(&addressLatch), sizeof(addressLatch));
        os.write(reinterpret_cast<const char *>(&vramAddr), sizeof(vramAddr));
        os.write(reinterpret_cast<const char *>(&tempAddr), sizeof(tempAddr));
        os.write(reinterpret_cast<const char *>(&fineX), sizeof(fineX));
        os.write(reinterpret_cast<const char *>(&dataBuffer), sizeof(dataBuffer));
        os.write(reinterpret_cast<const char *>(&scanline), sizeof(scanline));
        os.write(reinterpret_cast<const char *>(&cycle), sizeof(cycle));
        os.write(reinterpret_cast<const char *>(&frameCounter), sizeof(frameCounter));
        os.write(reinterpret_cast<const char *>(&nmi), sizeof(nmi));

        // Save pipeline state
        os.write(reinterpret_cast<const char *>(&bgNextTileId), sizeof(bgNextTileId));
        os.write(reinterpret_cast<const char *>(&bgNextTileAttr), sizeof(bgNextTileAttr));
        os.write(reinterpret_cast<const char *>(&bgNextTileLsb), sizeof(bgNextTileLsb));
        os.write(reinterpret_cast<const char *>(&bgNextTileMsb), sizeof(bgNextTileMsb));
        os.write(reinterpret_cast<const char *>(&bgShifterPatternLow), sizeof(bgShifterPatternLow));
        os.write(reinterpret_cast<const char *>(&bgShifterPatternHigh), sizeof(bgShifterPatternHigh));
        os.write(reinterpret_cast<const char *>(&bgShifterAttrLow), sizeof(bgShifterAttrLow));
        os.write(reinterpret_cast<const char *>(&bgShifterAttrHigh), sizeof(bgShifterAttrHigh));

        os.write(reinterpret_cast<const char *>(&sprite0HitDetectedThisScanline), sizeof(sprite0HitDetectedThisScanline));
        os.write(reinterpret_cast<const char *>(&scanlineSpriteCount), sizeof(scanlineSpriteCount));
        os.write(reinterpret_cast<const char *>(&nextScanlineSpriteCount), sizeof(nextScanlineSpriteCount));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationIndex), sizeof(spriteEvaluationIndex));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationStartIndex), sizeof(spriteEvaluationStartIndex));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationByteIndex), sizeof(spriteEvaluationByteIndex));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationBytesCopied), sizeof(spriteEvaluationBytesCopied));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationLatch), sizeof(spriteEvaluationLatch));
        os.write(reinterpret_cast<const char *>(&secondaryOamAddress), sizeof(secondaryOamAddress));
        os.write(reinterpret_cast<const char *>(&spriteEvaluationComplete), sizeof(spriteEvaluationComplete));
        os.write(reinterpret_cast<const char *>(secondaryOam.data()), secondaryOam.size());
        os.write(reinterpret_cast<const char *>(currentSprites.data()), sizeof(currentSprites));
        os.write(reinterpret_cast<const char *>(nextSprites.data()), sizeof(nextSprites));
        os.write(reinterpret_cast<const char *>(oamMemory.data()), oamMemory.size());
        os.write(reinterpret_cast<const char *>(paletteTable.data()), paletteTable.size());
        os.write(reinterpret_cast<const char *>(frameBuffer.data()), frameBuffer.size() * sizeof(frameBuffer[0]));
        os.write(reinterpret_cast<const char *>(&tilesEnabled), sizeof(tilesEnabled));
        os.write(reinterpret_cast<const char *>(&spritesEnabled), sizeof(spritesEnabled));
    }

    void PPU::loadState(std::istream &is, bool includesFrameBuffer)
    {
        vram.loadState(is);
        is.read(reinterpret_cast<char *>(&ppuCtrl), sizeof(ppuCtrl));
        is.read(reinterpret_cast<char *>(&ppuMask), sizeof(ppuMask));
        is.read(reinterpret_cast<char *>(&ppuStatus), sizeof(ppuStatus));
        is.read(reinterpret_cast<char *>(&oamAddr), sizeof(oamAddr));
        is.read(reinterpret_cast<char *>(&oamDataBusLatch), sizeof(oamDataBusLatch));
        is.read(reinterpret_cast<char *>(&addressLatch), sizeof(addressLatch));
        is.read(reinterpret_cast<char *>(&vramAddr), sizeof(vramAddr));
        is.read(reinterpret_cast<char *>(&tempAddr), sizeof(tempAddr));
        is.read(reinterpret_cast<char *>(&fineX), sizeof(fineX));
        is.read(reinterpret_cast<char *>(&dataBuffer), sizeof(dataBuffer));
        is.read(reinterpret_cast<char *>(&scanline), sizeof(scanline));
        is.read(reinterpret_cast<char *>(&cycle), sizeof(cycle));
        is.read(reinterpret_cast<char *>(&frameCounter), sizeof(frameCounter));
        is.read(reinterpret_cast<char *>(&nmi), sizeof(nmi));

        // Load pipeline state
        is.read(reinterpret_cast<char *>(&bgNextTileId), sizeof(bgNextTileId));
        is.read(reinterpret_cast<char *>(&bgNextTileAttr), sizeof(bgNextTileAttr));
        is.read(reinterpret_cast<char *>(&bgNextTileLsb), sizeof(bgNextTileLsb));
        is.read(reinterpret_cast<char *>(&bgNextTileMsb), sizeof(bgNextTileMsb));
        is.read(reinterpret_cast<char *>(&bgShifterPatternLow), sizeof(bgShifterPatternLow));
        is.read(reinterpret_cast<char *>(&bgShifterPatternHigh), sizeof(bgShifterPatternHigh));
        is.read(reinterpret_cast<char *>(&bgShifterAttrLow), sizeof(bgShifterAttrLow));
        is.read(reinterpret_cast<char *>(&bgShifterAttrHigh), sizeof(bgShifterAttrHigh));

        is.read(reinterpret_cast<char *>(&sprite0HitDetectedThisScanline), sizeof(sprite0HitDetectedThisScanline));
        is.read(reinterpret_cast<char *>(&scanlineSpriteCount), sizeof(scanlineSpriteCount));
        is.read(reinterpret_cast<char *>(&nextScanlineSpriteCount), sizeof(nextScanlineSpriteCount));
        is.read(reinterpret_cast<char *>(&spriteEvaluationIndex), sizeof(spriteEvaluationIndex));
        is.read(reinterpret_cast<char *>(&spriteEvaluationStartIndex), sizeof(spriteEvaluationStartIndex));
        is.read(reinterpret_cast<char *>(&spriteEvaluationByteIndex), sizeof(spriteEvaluationByteIndex));
        is.read(reinterpret_cast<char *>(&spriteEvaluationBytesCopied), sizeof(spriteEvaluationBytesCopied));
        is.read(reinterpret_cast<char *>(&spriteEvaluationLatch), sizeof(spriteEvaluationLatch));
        is.read(reinterpret_cast<char *>(&secondaryOamAddress), sizeof(secondaryOamAddress));
        is.read(reinterpret_cast<char *>(&spriteEvaluationComplete), sizeof(spriteEvaluationComplete));
        is.read(reinterpret_cast<char *>(secondaryOam.data()), secondaryOam.size());
        is.read(reinterpret_cast<char *>(currentSprites.data()), sizeof(currentSprites));
        is.read(reinterpret_cast<char *>(nextSprites.data()), sizeof(nextSprites));
        is.read(reinterpret_cast<char *>(oamMemory.data()), oamMemory.size());
        is.read(reinterpret_cast<char *>(paletteTable.data()), paletteTable.size());
        if (includesFrameBuffer)
            is.read(reinterpret_cast<char *>(frameBuffer.data()), frameBuffer.size() * sizeof(frameBuffer[0]));
        is.read(reinterpret_cast<char *>(&tilesEnabled), sizeof(tilesEnabled));
        is.read(reinterpret_cast<char *>(&spritesEnabled), sizeof(spritesEnabled));
    }
}
