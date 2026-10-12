#include "Core/PPU/PPU.h"
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

int main()
{
    try
    {
        for (bool unlimited : {false, true})
            for (bool tall : {false, true})
                for (uint8_t mask : {0x00, 0x08, 0x10, 0x18})
                    for (int count : {8, 9, 64})
                        testOverflow(unlimited, count, mask, tall);
        std::cout << "PPU sprite overflow tests passed (48 cases)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
