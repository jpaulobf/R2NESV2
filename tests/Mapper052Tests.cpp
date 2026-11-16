#include "Core/Cartridge/Mappers/Mapper052.h"
#include "TestFixtures.h"
#include <sstream>
#include <set>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool b, const char *s)
{
    if (!b)
        throw std::runtime_error(s);
}
static void banks()
{
    Mapper052 m(64, 128, MirrorMode::HORIZONTAL, 8192, true);
    uint32_t a = 0;
    uint8_t d = 0;
    auto w = [&](uint16_t x, uint8_t v)
    { check(m.cpuMapWrite(x, a, v, 0), "write"); };
    auto p = [&](uint16_t x)
    { check(m.cpuMapRead(x,a,d),"PRG"); return a; };
    auto c = [&](uint16_t x)
    { check(m.ppuMapRead(x,a,d,0),"CHR"); return a; };
    check(p(0xC000) == 0x3C000 && p(0xE000) == 0x3E000, "initial fixed banks");
    w(0x8000, 6);
    w(0x8001, 0x13);
    w(0x8000, 2);
    w(0x8001, 0x85);
    check(p(0x8000) == 0x26000 && c(0x1000) == 0x21400, "MMC3 bank registers");
    w(0xA001, 0xC0);
    w(0x6000, 0xFB);
    check(p(0x8000) == 0x26000 && c(0x1000) == 0x21400, "write-protected outer register");
    w(0xA001, 0);
    w(0x6000, 0xFB);
    check(p(0x8000) == 0x26000, "disabled outer register");
    w(0xA001, 0x80);
    w(0x6123, 0xFB);
    check(p(0x8000) == 0x66000 && p(0xE000) == 0x7E000 && c(0x1000) == 0x61400, "128 KiB outer banks");
    check(m.cpuMapRead(0x6123, a, d) && d == 0xFB, "outer write overlaps RAM");
    w(0x6123, 0x80);
    check(p(0x8000) == 0x66000, "outer lock");
    check(m.cpuMapRead(0x6123, a, d) && d == 0x80, "locked outer must not lock RAM");
    auto bat = m.getBatteryData();
    check(bat.size() == 8192 && bat[0x123] == 0x80, "battery data");
    std::stringstream state;
    m.saveState(state);
    m.reset();
    check(p(0x8000) == 0, "reset");
    state.seekg(0);
    m.loadState(state);
    check(bool(state) && p(0x8000) == 0x66000, "state restore");
    auto bad = state.str();
    bad.pop_back();
    std::stringstream broken(bad);
    m.loadState(broken);
    check(!broken && p(0x8000) == 0x66000, "truncated state");
    m.reset();
    w(0x6000, 0x04);
    check(p(0xE000) == 0xBE000 && c(0) == 0x80000, "256 KiB outer banks");
}
static void headers()
{
    std::vector<uint8_t> rom(16 + 64 * 0x4000 + 128 * 0x2000);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 64;
    rom[5] = 128;
    rom[6] = 0x42;
    rom[7] = 0x38;
    rom[10] = 0x70;
    check(FixtureCartridge(rom).isValid(), "NES 2.0 image rejected");
    rom[8] = 0xD0;
    check(!FixtureCartridge(rom).isValid(), "unsupported submapper");
    rom[8] = 0;
    rom.pop_back();
    check(!FixtureCartridge(rom).isValid(), "truncated image");
}
static void legacyChrOrder(const char *path)
{
    auto rom = fixtureRom(path);
    FixtureCartridge cart(rom);
    check(cart.isValid() && cart.getRomHash() == "68FE207F", "unexpected legacy ROM");
    // Read each physical 128 KiB block through the normal mapper, comparing
    // with the old dump's A18/A19-swapped source block.
    for (unsigned bank = 0; bank < 8; ++bank)
    {
        cart.getMapper()->reset();
        const uint8_t reg = 0x40 | ((bank & 1) << 4) | ((bank & 2) << 4) | (bank & 4);
        cart.cpuWrite(0x6000, reg, 0);
        const unsigned source = (bank & 1) | ((bank & 2) << 1) | ((bank & 4) >> 1);
        for (unsigned i = 0; i < 0x400; ++i)
        {
            uint8_t actual = 0;
            check(cart.ppuPeek(i, actual, 0) && actual == rom[16 + 0x100000 + source * 0x20000 + i],
                  "legacy CHR bank normalization");
        }
    }
    // A different payload must keep the physical mapper's normal bank order.
    rom[16] ^= 1;
    FixtureCartridge other(rom);
    check(other.isValid() && other.getRomHash() != "68FE207F", "modified ROM identity");
    other.cpuWrite(0x6000, 0x60, 0); // CHR physical block 2.
    for (unsigned i = 0; i < 0x400; ++i)
    {
        uint8_t actual = 0;
        check(other.ppuPeek(i, actual, 0) && actual == rom[16 + 0x100000 + 2 * 0x20000 + i],
              "other ROM bank order changed");
    }
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 52, "ROM rejected");
    unsigned frames = 0;
    std::string before;
    for (unsigned step = 0; step < 30000000 && frames < 600; ++step)
    {
        nes.step(false);
        if (!nes.isFrameComplete())
            continue;
        nes.clearFrameComplete();
        ++frames;
        if (frames == 120)
        {
            std::ostringstream s;
            nes.getBus().cart->getMapper()->saveState(s);
            before = s.str();
            nes.getJoysticks().controller1.setButton(IO::BUTTON_DOWN, true);
        }
        if (frames == 125)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_DOWN, false);
        if (frames == 180)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, true);
        if (frames == 185)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, false);
    }
    const auto *px = nes.getPpu().getFrameBuffer();
    std::set<uint32_t> colors(px, px + 256 * 240);
    std::ostringstream after;
    nes.getBus().cart->getMapper()->saveState(after);
    check(frames == 600 && colors.size() > 2 && !before.empty() && before.back() != after.str().back(), "menu selection failed");
    std::stringstream saved;
    check(nes.saveState(saved), "save");
    auto expected = saved.str();
    for (int i = 0; i < 10000; i++)
        nes.step(false);
    saved.seekg(0);
    check(nes.loadState(saved), "load");
    std::stringstream restored;
    check(nes.saveState(restored) && restored.str() == expected, "state mismatch");
    std::cout << "600 frames, " << colors.size() << " colors, outer=" << unsigned(uint8_t(after.str().back())) << ", CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        banks();
        headers();
        if (argc == 2)
        {
            legacyChrOrder(argv[1]);
            smoke(argv[1]);
        }
        std::cout << "Mapper 52 tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}