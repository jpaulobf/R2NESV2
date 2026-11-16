#include "TestFixtures.h"
#include <set>
#include <sstream>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}
static std::vector<uint8_t> image(bool nes2 = false)
{
    std::vector<uint8_t> r(16 + 8 * 0x4000 + 64 * 0x2000);
    r[0] = 'N';
    r[1] = 'E';
    r[2] = 'S';
    r[3] = 0x1A;
    r[4] = 8;
    r[5] = 64;
    r[6] = 0xB0;
    r[7] = nes2 ? 0x18 : 0x10;
    for (unsigned bank = 0; bank < 512; ++bank)
    {
        std::fill_n(r.begin() + 16 + 8 * 0x4000 + bank * 1024, 1024, uint8_t(bank));
        r[16 + 8 * 0x4000 + bank * 1024 + 1] = uint8_t(bank >> 8);
    }
    return r;
}
static void checkBoard()
{
    auto cart = std::make_shared<FixtureCartridge>(image());
    FixtureNES nes("");
    check(cart->isValid() && nes.insertCartridge(cart) && cart->getMapperId() == 27, "mapper 27 rejected");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    bus.cpuWrite(0xB000, 3);
    bus.cpuWrite(0xB001, 0x10);
    check(ppu.ppuRead(0) == 3 && ppu.ppuRead(1) == 1, "9-bit CHR bank");
    bus.cpuWrite(0xB004, 7);
    check(ppu.ppuRead(0) == 7 && ppu.ppuRead(1) == 1, "A2 must not select a different register");
    bus.cpuWrite(0xB002, 5);
    bus.cpuWrite(0xB003, 2);
    check(ppu.ppuRead(0x400) == 0x25, "second CHR bank");
    bus.cpuWrite(0x9000, 3);
    check(cart->getMirrorMode() == MirrorMode::ONESCREEN_HI, "mapper mirroring");
    bus.cpuWrite(0xF000, 15);
    bus.cpuWrite(0xF001, 15);
    bus.cpuWrite(0xF002, 6);
    cart->getMapper()->tick();
    check(cart->getMapper()->getIrqFlag(), "cycle IRQ");
    bus.cpuWrite(0xF003, 0);
    check(!cart->getMapper()->getIrqFlag(), "IRQ acknowledge");
    std::stringstream save;
    check(nes.saveState(save), "save");
    auto expected = save.str();
    bus.cpuWrite(0xB001, 0);
    save.seekg(0);
    check(nes.loadState(save), "load");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == expected && ppu.ppuRead(1) == 1, "state restore");
    auto broken = expected;
    broken.resize(broken.size() - 15);
    std::stringstream invalid(broken);
    check(!nes.loadState(invalid), "truncated state accepted");
}
static void headers()
{
    auto r = image(true);
    check(FixtureCartridge(r).isValid(), "NES 2.0 header rejected");
    r[8] = 0x10;
    check(!FixtureCartridge(r).isValid(), "unsupported submapper");
    r = image();
    r.pop_back();
    check(!FixtureCartridge(r).isValid(), "truncated CHR");
    r = image();
    r.resize(16 + 7 * 0x4000);
    check(!FixtureCartridge(r).isValid(), "truncated PRG");
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 27, "ROM rejected");
    unsigned frames = 0;
    for (unsigned i = 0; i < 40000000 && frames < 900; ++i)
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
        if (frames == 500)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, true);
        if (frames == 505)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, false);
    }
    const auto *p = nes.getPpu().getFrameBuffer();
    std::set<uint32_t> colors(p, p + 256 * 240);
    check(frames == 900 && colors.size() > 2, "ROM did not produce visible video");
    std::stringstream saved;
    check(nes.saveState(saved), "ROM save");
    auto expected = saved.str();
    for (int i = 0; i < 10000; ++i)
        nes.step(false);
    saved.seekg(0);
    check(nes.loadState(saved), "ROM load");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == expected, "ROM state mismatch");
    std::cout << "900 frames, " << colors.size() << " colors, CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        checkBoard();
        headers();
        if (argc == 2)
            smoke(argv[1]);
        std::cout << "Mapper 27 tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}