#include "Core/PPU/PPU.h"
#include "TestFixtures.h"
#include <sstream>
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

static void testOverflowFlagPhaseAndState()
{
    PPU ppu;
    ppu.reset();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2003, 0);
    for (int sprite = 0; sprite < 64; ++sprite)
    {
        ppu.cpuWrite(0x2004, sprite < 9 ? 40 : 0xFF);
        for (int byte = 0; byte < 3; ++byte)
            ppu.cpuWrite(0x2004, 0);
    }
    for (int dot = 0; dot < 341; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0x18);
    for (int dot = 0; dot < 40 * 341 + 130; ++dot)
        ppu.clock();
    check((ppu.cpuRead(0x2002, true) & 0x20) == 0, "overflow set before ninth sprite evaluation");
    ppu.clock(); // Dot 130: a avaliação encontra o nono sprite.
    check((ppu.cpuRead(0x2002, true) & 0x20) == 0, "overflow exposed on evaluation dot");
    std::stringstream checkpoint;
    ppu.saveState(checkpoint);
    ppu.clock();
    check((ppu.cpuRead(0x2002, true) & 0x20) != 0, "overflow missing on next dot");
    ppu.loadState(checkpoint);
    check(bool(checkpoint), "pending overflow state rejected");
    check((ppu.cpuRead(0x2002, true) & 0x20) == 0, "restored overflow appeared early");
    ppu.clock();
    check((ppu.cpuRead(0x2002, true) & 0x20) != 0, "restored overflow did not appear");
}

static void testCpuStatusReadPhase()
{
    for (int offset = 0; offset < 3; ++offset)
    {
        std::vector<uint8_t> rom(16 + 16384, 0xEA);
        std::fill_n(rom.begin(), 16, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1A;
        rom[4] = 1;
        rom[16] = 0xAD;
        rom[17] = 0x02;
        rom[18] = 0x20; // LDA $2002
        rom[16 + 0x3FFC] = 0;
        rom[16 + 0x3FFD] = 0x80;
        FixtureNES nes("");
        auto cartridge = std::make_shared<FixtureCartridge>(rom);
        check(cartridge->isValid() && nes.insertCartridge(cartridge), "status read fixture failed");
        auto &cpu = nes.getCpu();
        auto &ppu = nes.getPpu();
        while (!cpu.complete())
            cpu.clock();
        for (int i = 0; i < 3; ++i)
            cpu.clock(); // A próxima leitura da CPU é $2002.
        for (int dot = 0; dot < 262 * 341; ++dot)
            ppu.clock();
        ppu.cpuWrite(0x2003, 0);
        for (int sprite = 0; sprite < 64; ++sprite)
        {
            ppu.cpuWrite(0x2004, sprite < 9 ? 40 : 0xFF);
            for (int byte = 0; byte < 3; ++byte)
                ppu.cpuWrite(0x2004, 0);
        }
        for (int dot = 0; dot < 341; ++dot)
            ppu.clock();
        ppu.cpuWrite(0x2001, 0x18);
        for (int dot = 0; dot < 260 * 341 + 340 + offset; ++dot)
            ppu.clock();
        check((ppu.cpuRead(0x2002, true) & 0xA0) == 0xA0, "status fixture did not set both flags");
        nes.step(false);
        check((cpu.a & 0xA0) == (offset == 0 ? 0xA0 : 0x80), "CPU sampled status flags at wrong phases");
        check((ppu.cpuRead(0x2002, true) & 0x80) == 0, "CPU status read did not clear vblank");
    }
}

static void testSecondaryOamReadAfterFetch(bool interruptFetch)
{
    PPU ppu;
    ppu.reset();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2003, 0);
    for (int sprite = 0; sprite < 64; ++sprite)
    {
        const uint8_t y = interruptFetch ? (sprite < 8 ? sprite : 0xFF)
                                         : (sprite == 63 ? 0x5A : 0xFF);
        ppu.cpuWrite(0x2004, y);
        ppu.cpuWrite(0x2004, static_cast<uint8_t>(sprite * 32 + 1));
        ppu.cpuWrite(0x2004, static_cast<uint8_t>(sprite * 32 + 2));
        ppu.cpuWrite(0x2004, static_cast<uint8_t>(sprite * 32 + 3));
    }
    // Evita o dot omitido no pré-render e liga antes da linha examinada.
    for (int dot = 0; dot < 341; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0x18);
    if (!interruptFetch)
    {
        for (int dot = 0; dot < 324; ++dot)
            ppu.clock();
        check(ppu.cpuRead(0x2004, true) == 0x5A,
              "last rejected Y was not visible from secondary OAM");
        return;
    }

    for (int dot = 0; dot < 7 * 341 + 268; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0);
    for (int dot = 0; dot < 18; ++dot)
        ppu.clock();
    std::stringstream checkpoint;
    ppu.saveState(checkpoint);
    const auto finish = [&]()
    {
        ppu.cpuWrite(0x2001, 0x18);
        for (int dot = 0; dot < 38; ++dot)
            ppu.clock();
        check(ppu.cpuRead(0x2004, true) == 6,
              "secondary OAM fetch address did not pause with rendering");
    };
    finish();
    ppu.loadState(checkpoint);
    check(bool(checkpoint), "interrupted OAM fetch state rejected");
    finish();
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

static void testSpriteStartAtDot339(bool disableAt339)
{
    std::vector<uint8_t> rom(16 + 16384, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;
    FixtureNES nes("");
    auto cartridge = std::make_shared<FixtureCartridge>(rom);
    check(cartridge->isValid() && nes.insertCartridge(cartridge), "sprite fixture failed");
    auto &ppu = nes.getPpu();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    for (int row = 0; row < 8; ++row)
        ppu.ppuWrite(row, 0xFF);
    ppu.ppuWrite(0x3F00, 0x0F);
    ppu.ppuWrite(0x3F11, 0x30);
    ppu.cpuWrite(0x2003, 0);
    for (int i = 0; i < 64; ++i)
    {
        ppu.cpuWrite(0x2004, i == 0 ? 0 : 0xFF);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 128);
    }
    ppu.cpuWrite(0x2001, 0x14);
    // No dot 330 da linha 0, a busca do sprite da linha 1 já terminou.
    for (int dot = 0; dot < 341 + 330; ++dot)
        ppu.clock();
    if (disableAt339)
        ppu.cpuWrite(0x2001, 0);
    for (int dot = 0; dot < 12; ++dot)
        ppu.clock(); // Processa dot 339 e entra na linha 1.
    std::stringstream checkpoint;
    ppu.saveState(checkpoint);
    ppu.cpuWrite(0x2001, 0x14);
    for (int dot = 0; dot < 140; ++dot)
        ppu.clock();
    const auto *frame = ppu.getFrameBuffer();
    const uint32_t early = frame[256 + 5], atX = frame[256 + 128], background = frame[256 + 30];
    check(disableAt339 ? early != background && atX == background
                       : early == background && atX != background,
          "sprite X start did not follow rendering at dot 339");
    ppu.loadState(checkpoint);
    check(bool(checkpoint), "sprite dot 339 state rejected");
    ppu.cpuWrite(0x2001, 0x14);
    for (int dot = 0; dot < 140; ++dot)
        ppu.clock();
    frame = ppu.getFrameBuffer();
    check(frame[256 + 5] == early && frame[256 + 128] == atX,
          "sprite dot 339 state did not restore");
}

static void testSpriteBlanking(bool blankAfterStart)
{
    std::vector<uint8_t> rom(16 + 16384, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;
    FixtureNES nes("");
    auto cartridge = std::make_shared<FixtureCartridge>(rom);
    check(cartridge->isValid() && nes.insertCartridge(cartridge), "sprite blanking fixture failed");
    auto &ppu = nes.getPpu();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    for (int row = 0; row < 8; ++row)
        ppu.ppuWrite(row, 0xFF);
    ppu.ppuWrite(0x3F00, 0x0F);
    ppu.ppuWrite(0x3F11, 0x30);
    ppu.cpuWrite(0x2003, 0);
    for (int i = 0; i < 64; ++i)
    {
        ppu.cpuWrite(0x2004, i == 0 ? 0 : 0xFF);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, blankAfterStart ? 31 : 100);
    }
    ppu.cpuWrite(0x2001, 0x14);
    for (int dot = 0; dot < 2 * 341 + 32; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0);
    for (int dot = 0; dot < 30; ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0x14);
    for (int dot = 0; dot < 90; ++dot)
        ppu.clock();
    const auto *frame = ppu.getFrameBuffer();
    const uint32_t background = frame[256 + 20];
    if (blankAfterStart)
        check(frame[256 + 66] != background, "sprite shifter did not pause during blanking");
    else
        check(frame[256 + 100] != background && frame[256 + 130] == background,
              "sprite X counter stopped during blanking");
}

static void testSpriteSurvivesBlankLines(bool interruptFetch)
{
    std::vector<uint8_t> rom(16 + 16384, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;
    FixtureNES nes("");
    auto cartridge = std::make_shared<FixtureCartridge>(rom);
    check(cartridge->isValid() && nes.insertCartridge(cartridge), "stale sprite fixture failed");
    auto &ppu = nes.getPpu();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    for (int row = 0; row < 8; ++row)
        ppu.ppuWrite(row, 0xFF);
    ppu.ppuWrite(0x3F00, 0x0F);
    ppu.ppuWrite(0x3F11, 0x30);
    ppu.cpuWrite(0x2003, 0);
    for (int i = 0; i < 64; ++i)
    {
        ppu.cpuWrite(0x2004, i == 0 ? 0 : 0xFF);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, 0);
        ppu.cpuWrite(0x2004, interruptFetch ? 255 : 48);
    }
    ppu.cpuWrite(0x2001, 0x14);
    if (interruptFetch)
    {
        for (int dot = 0; dot < 341 + 265; ++dot)
            ppu.clock();
        std::stringstream duringFetch;
        ppu.saveState(duringFetch);
        for (int dot = 0; dot < 100; ++dot)
            ppu.clock();
        std::stringstream expected;
        ppu.saveState(expected);
        ppu.loadState(duringFetch);
        check(bool(duringFetch), "sprite fetch state rejected");
        for (int dot = 0; dot < 100; ++dot)
            ppu.clock();
        std::stringstream actual;
        ppu.saveState(actual);
        check(actual.str() == expected.str(), "sprite fetch state did not restore");
        duringFetch.clear();
        duringFetch.seekg(0);
        ppu.loadState(duringFetch);
        check(bool(duringFetch), "sprite fetch state could not be restored twice");
        for (int dot = 0; dot < 341 - 265; ++dot)
            ppu.clock();
    }
    for (int dot = 0; dot < (interruptFetch ? 257 : 2 * 341); ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2001, 0);
    for (int dot = 0; dot < (interruptFetch ? 73 : 3 * 341 + 330); ++dot)
        ppu.clock();
    std::stringstream checkpoint;
    ppu.saveState(checkpoint);
    auto finish = [&]()
    {
        ppu.cpuWrite(0x2001, 0x14);
        for (int dot = 0; dot < 341; ++dot)
            ppu.clock();
        const auto *frame = ppu.getFrameBuffer();
        const int line = interruptFetch ? 2 : 5;
        check(frame[line * 256] != frame[line * 256 + 20], "sprite shifter lost across blank scanlines");
    };
    finish();
    ppu.loadState(checkpoint);
    check(bool(checkpoint), "stale sprite state rejected");
    finish();
}

static void testSpriteResizeDuringFetch(bool flip, bool resizeBeforeFetch)
{
    std::vector<uint8_t> rom(16 + 16384, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 1;
    FixtureNES nes("");
    auto cartridge = std::make_shared<FixtureCartridge>(rom);
    check(cartridge->isValid() && nes.insertCartridge(cartridge), "sprite resize fixture failed");
    auto &ppu = nes.getPpu();
    for (int dot = 0; dot < 262 * 341; ++dot)
        ppu.clock();
    // Na linha 12, o sprite de Y=0 usa a linha 12. Em 8x8 ela
    // corresponde à linha 4 do tile 2, ou à linha 3 com flip vertical.
    ppu.ppuWrite(2 * 16 + 4, 0x80);
    ppu.ppuWrite(2 * 16 + 3, 0x40);
    ppu.ppuWrite(3 * 16 + 4, 0x40);
    ppu.ppuWrite(0x3F00, 0x0F);
    ppu.ppuWrite(0x3F11, 0x30);
    ppu.cpuWrite(0x2003, 0);
    for (int i = 0; i < 64; ++i)
    {
        ppu.cpuWrite(0x2004, i == 0 ? 0 : 0xFF);
        ppu.cpuWrite(0x2004, 2);
        ppu.cpuWrite(0x2004, flip ? 0x80 : 0);
        ppu.cpuWrite(0x2004, 32);
    }
    ppu.cpuWrite(0x2000, 0x20);
    ppu.cpuWrite(0x2001, 0x14);
    for (int dot = 0; dot < 13 * 341 + (resizeBeforeFetch ? 260 : 262); ++dot)
        ppu.clock();
    ppu.cpuWrite(0x2000, 0);
    for (int dot = 0; dot < 341; ++dot)
        ppu.clock();
    const auto *frame = ppu.getFrameBuffer();
    const uint32_t background = frame[13 * 256 + 20];
    const bool pixel0 = frame[13 * 256 + 32] != background;
    const bool pixel1 = frame[13 * 256 + 33] != background;
    const bool expectFirst = !flip && resizeBeforeFetch;
    check(pixel0 == expectFirst && pixel1 != expectFirst,
          "sprite resize fetched an incorrect pattern row");
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
        testOverflowFlagPhaseAndState();
        testCpuStatusReadPhase();
        testSecondaryOamReadAfterFetch(false);
        testSecondaryOamReadAfterFetch(true);
        testSpriteStartAtDot339(false);
        testSpriteStartAtDot339(true);
        testSpriteBlanking(false);
        testSpriteBlanking(true);
        testSpriteSurvivesBlankLines(false);
        testSpriteSurvivesBlankLines(true);
        for (bool flip : {false, true})
            for (bool before : {false, true})
                testSpriteResizeDuringFetch(flip, before);
        std::cout << "PPU sprite overflow tests passed (48 cases)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}