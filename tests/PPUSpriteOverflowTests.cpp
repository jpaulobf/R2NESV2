#include "Core/PPU/PPU.h"
#include "TestFixtures.h"
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void testOverflow(bool unlimited, int spriteCount, uint8_t mask, bool tall)
{
    PPU ppu;
    ppu.reset();
    ppu.setUnlimitedSprites(unlimited);
    // Um frame completo com rendering desligado libera as escritas após reset.
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    ppu.clearFrameComplete();

    ppu.cpuWrite(0x2003, 0);
    for (int sprite = 0; sprite < 64; ++sprite)
    {
        ppu.cpuWrite(0x2004, sprite < spriteCount ? 40 : 0xFF);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 0);
    }
    ppu.cpuWrite(0x2000, tall ? 0x20 : 0);
    ppu.cpuWrite(0x2001, mask);
    // O primeiro sprite está fora de faixa antes da scanline 41.
    for (int dot = 0; dot < 40 * 341; ++dot)
        ppu.clock();
    check((ppu.cpuRead(0x2002) & 0x20) == 0, "Overflow before sprites are in range");
    for (int dot = 0; dot < 4 * 341; ++dot)
        ppu.clock();
    const bool expected = spriteCount > 8 && (mask & 0x18) != 0;
    check(((ppu.cpuRead(0x2002) & 0x20) != 0) == expected, "Sprite overflow mismatch");
    check(((ppu.cpuRead(0x2002) & 0x20) != 0) == expected, "Status read cleared overflow");

    // Mesmo desligando rendering, o flag permanece até o pré-render seguinte.
    ppu.cpuWrite(0x2001, 0);
    while (!ppu.isFrameComplete())
        ppu.clock();
    check(((ppu.cpuRead(0x2002) & 0x20) != 0) == expected, "Overflow did not persist to vblank");
    // De scanline 241/dot 2 até processar pré-render/dot 1.
    for (int dot = 0; dot < 20 * 341; ++dot)
        ppu.clock();
    check((ppu.cpuRead(0x2002) & 0x20) == 0, "Pre-render did not clear overflow");
}

static void testBackgroundPipelineWithSpritesOnly()
{
    // NROM com CHR RAM: o tile de fundo contém pixels claros e escuros.
    std::vector<uint8_t> rom(16 + 16384, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;
    FixtureNES spritesOnly("");
    FixtureNES reference("");
    for (FixtureNES *nes : {&spritesOnly, &reference})
    {
        auto cartridge = std::make_shared<FixtureCartridge>(rom);
        check(cartridge->isValid() && nes->insertCartridge(cartridge), "NROM fixture failed");
        auto &ppu = nes->getPpu();
        for (int dot = 0; dot < 262 * 341; ++dot)
            ppu.clock(); // Libera os registradores após o reset.
        for (int row = 0; row < 8; ++row)
            ppu.ppuWrite(row, 0x96);
        ppu.ppuWrite(0x3F00, 0x0F);
        ppu.ppuWrite(0x3F01, 0x30);
        ppu.cpuWrite(0x2001, nes == &spritesOnly ? 0x10 : 0x18);
        for (int dot = 0; dot < 341 + 50; ++dot)
            ppu.clock();
    }

    // O fundo volta a ficar visível no meio da linha. Seus pixels seguintes
    // devem coincidir com os de uma PPU cujo fundo esteve sempre habilitado.
    spritesOnly.getPpu().cpuWrite(0x2001, 0x18);
    for (int dot = 0; dot < 12; ++dot)
    {
        spritesOnly.getPpu().clock();
        reference.getPpu().clock();
    }
    const auto *actual = spritesOnly.getPpu().getFrameBuffer();
    const auto *expected = reference.getPpu().getFrameBuffer();
    bool sawBothColors = false;
    for (int x = 54; x < 61; ++x)
    {
        check(actual[x] == expected[x], "background pipeline stopped with sprites only");
        sawBothColors |= expected[x] != expected[54];
    }
    check(sawBothColors, "background fixture did not produce a varied pattern");
}

int main()
{
    try
    {
        for (bool unlimited : {false, true})
            for (bool tall : {false, true})
                for (uint8_t mask : {0x00, 0x08, 0x10, 0x18})
                    for (int count : {8, 9, 64})
                        testOverflow(unlimited, count, mask, tall);
        testBackgroundPipelineWithSpritesOnly();
        std::cout << "PPU sprite overflow tests passed (48 cases)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}