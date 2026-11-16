#include "Core/Cartridge/Mappers/Mapper210.h"
#include "TestFixtures.h"
#include <algorithm>
#include <sstream>
#include <set>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool v, const char *msg)
{
    if (!v)
        throw std::runtime_error(msg);
}
static void exercise()
{
    Mapper210 m(8, 32, MirrorMode::HORIZONTAL, false, 0, false);
    uint32_t p = 0;
    uint8_t d = 0;
    auto w = [&](uint16_t a, uint8_t v)
    { check(m.cpuMapWrite(a, p, v, 0), "write"); };
    auto prg = [&](uint16_t a)
    { check(m.cpuMapRead(a,p,d), "PRG read"); return p / 8192; };
    for (unsigned i = 0; i < 8; ++i)
    {
        w(0x8000 + i * 0x800 + 0x345, 0xE0 + i);
        check(m.ppuMapRead(i * 0x400, p, d, 0) && p / 1024 == 0xE0 + i, "CHR bank");
    }
    w(0xE000, 0x42);
    w(0xE800, 0xC3);
    w(0xF000, 4);
    check(prg(0x8000) == 2 && prg(0xA000) == 3 && prg(0xC000) == 4 && prg(0xE000) == 15, "PRG banks");
    MirrorMode modes[] = {MirrorMode::ONESCREEN_LO, MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_HI};
    for (int i = 0; i < 4; i++)
    {
        w(0xE000, i << 6);
        check(m.getMirrorMode() == modes[i], "mirroring");
    }
    check(!m.cpuMapRead(0x6000, p, d), "340 RAM");
    Mapper210 n(8, 32, MirrorMode::HORIZONTAL, true, 8192, true);
    n.cpuMapWrite(0x6000, p, 0x12, 0);
    n.cpuMapRead(0x6000, p, d);
    check(d == 0, "RAM protection");
    n.cpuMapWrite(0xC123, p, 1, 0);
    n.cpuMapWrite(0x6000, p, 0x34, 0);
    n.cpuMapWrite(0xE000, p, 0x40, 0);
    check(n.getMirrorMode() == MirrorMode::HORIZONTAL, "175 mirroring");
    check(n.getBatteryData()[0] == 0x34, "battery data");
    std::stringstream state;
    n.saveState(state);
    n.cpuMapWrite(0x6000, p, 0x56, 0);
    state.seekg(0);
    n.loadState(state);
    check(bool(state) && n.getBatteryData()[0] == 0x34, "state restore");
    auto bad = state.str();
    bad.resize(10);
    std::stringstream truncated(bad);
    n.loadState(truncated);
    check(!truncated && n.getBatteryData()[0] == 0x34, "truncated state");
}
static void headers()
{
    std::vector<uint8_t> rom(16 + 8 * 0x4000 + 32 * 0x2000);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 8;
    rom[5] = 32;
    rom[6] = 0x20;
    rom[7] = 0xD8;
    for (uint8_t sub : {uint8_t(0), uint8_t(1), uint8_t(2)})
    {
        rom[8] = sub << 4;
        rom[10] = sub == 1 ? 7 : 0;
        FixtureCartridge cart(rom);
        check(cart.isValid() && cart.getMapperId() == 210, "NES 2.0 mapper 210 header");
    }
    rom[8] = 0x30;
    check(!FixtureCartridge(rom).isValid(), "invalid submapper");
    rom[8] = 0x20;
    rom[10] = 7;
    check(!FixtureCartridge(rom).isValid(), "340 with RAM");
    rom[8] = 0;
    rom[10] = 0;
    rom.pop_back();
    check(!FixtureCartridge(rom).isValid(), "truncated CHR");
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 90, "ROM rejected");
    unsigned frames = 0;
    for (unsigned i = 0; i < 30000000 && frames < 600; ++i)
    {
        nes.step(false);
        if (!nes.isFrameComplete())
            continue;
        nes.clearFrameComplete();
        ++frames;
        if (frames == 300)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, true);
        if (frames == 305)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, false);
    }
    const auto *pixels = nes.getPpu().getFrameBuffer();
    std::set<uint32_t> colors(pixels, pixels + 256 * 240);
    check(frames == 600 && colors.size() > 2, "ROM did not produce visible video");
    std::stringstream state;
    check(nes.saveState(state), "save");
    auto before = state.str();
    for (int i = 0; i < 10000; i++)
        nes.step(false);
    state.seekg(0);
    check(nes.loadState(state), "load");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == before, "state mismatch");
    std::cout << "600 frames, " << colors.size() << " colors, CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        exercise();
        headers();
        if (argc == 2)
            smoke(argv[1]);
        std::cout << "Mapper 210 tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}