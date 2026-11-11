#include "Core/Cartridge/Mappers/Mapper090.h"
#include "Core/NES.h"
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include "TestFixtures.h"

using namespace R2NES::Core;
static void check(bool b, const char *s)
{
    if (!b)
        throw std::runtime_error(s);
}
static void write(Mapper090 &m, uint16_t a, uint8_t v)
{
    uint32_t x = 0;
    m.cpuMapWrite(a, x, v, 0);
}
static uint32_t prg(Mapper090 &m, uint16_t a)
{
    uint32_t x = 0;
    uint8_t d = 0;
    check(m.cpuMapRead(a, x, d), "PRG read");
    return x;
}
static uint32_t chr(Mapper090 &m, uint16_t a)
{
    uint32_t x = 0;
    uint8_t d = 0;
    check(m.ppuMapRead(a, x, d, 0), "CHR read");
    return x;
}
int main(int argc, char **argv)
{
    try
    {
        Mapper090 m(32, 32);
        check(prg(m, 0xfffc) == 0x7fffc, "reset vector must use last bank");
        check(prg(m, 0xd000) == 0x7d000, "D000 reads PRG");
        write(m, 0x5800, 13);
        write(m, 0x5801, 21);
        uint32_t x = 0;
        uint8_t d = 0;
        check(m.cpuMapRead(0x5800, x, d) && d == 17, "multiply low");
        check(m.cpuMapRead(0x5801, x, d) && d == 1, "multiply high");
        write(m, 0x8000, 5);
        write(m, 0x8001, 6);
        write(m, 0x8002, 7);
        write(m, 0x8003, 8);
        write(m, 0xd000, 2);
        check(prg(m, 0x8000) == 5 * 0x2000 && prg(m, 0xe000) == 63 * 0x2000, "8k PRG banking");
        write(m, 0xd000, 0x1e);
        check(prg(m, 0xe000) == 8 * 0x2000, "switchable last PRG bank");
        write(m, 0x9002, 9);
        check(chr(m, 0x800) == 9 * 0x400, "1k CHR banking");
        write(m, 0xd001, 1);
        check(m.getMirrorMode() == MirrorMode::HORIZONTAL, "mirroring");
        write(m, 0xc001, 0x44);
        write(m, 0xc004, 7);
        write(m, 0xc005, 255);
        write(m, 0xc003, 0);
        m.tick();
        check(m.getIrqFlag(), "CPU IRQ");
        std::stringstream ss;
        m.saveState(ss);
        write(m, 0xc002, 0);
        check(!m.getIrqFlag(), "IRQ acknowledge");
        m.loadState(ss);
        check(ss && m.getIrqFlag(), "IRQ state restore");
        if (argc > 1)
        {
            FixtureNES nes("");
            nes.setCheatDirectory("");
            check(nes.insertCartridge(argv[1]), "ROM load");
            for (int i = 0; i < 2500000; ++i)
                nes.step(false);
            const auto *pixels = nes.getPpu().getFrameBuffer();
            std::set<uint32_t> colors(pixels, pixels + 256 * 240);
            std::cout << "ROM frame colors: " << colors.size() << '\n';
            check(colors.size() > 2, "ROM did not render a varied frame");
        }
        std::cout << "Mapper 90 tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}