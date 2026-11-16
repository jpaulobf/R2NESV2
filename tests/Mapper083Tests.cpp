#include "Core/Cartridge/Mappers/Mapper083.h"
#include "TestFixtures.h"
#include <set>
#include <sstream>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool b, const char *m)
{
    if (!b)
        throw std::runtime_error(m);
}
static void banks()
{
    Mapper083 m(16, 64, true, MirrorMode::VERTICAL);
    uint32_t p = 0;
    uint8_t d = 0;
    auto w = [&](uint16_t a, uint8_t v)
    { check(m.cpuMapWrite(a, p, v, 0), "register write"); };
    auto pr = [&](uint16_t a)
    { check(m.cpuMapRead(a,p,d),"PRG read"); return p/0x2000; };
    auto ch = [&](uint16_t a)
    { check(m.ppuMapRead(a,p,d,0),"CHR read"); return p/0x800; };
    check(pr(0xE000) == 31, "reset vector bank");
    w(0x8000, 3);
    check(pr(0x8000) == 6 && pr(0xA000) == 7 && pr(0xC000) == 30 && pr(0xE000) == 31, "16K bank mode");
    w(0x8100, 8);
    check(pr(0x8000) == 6 && pr(0xC000) == 6, "16K mirrored mode");
    w(0x8300, 5);
    w(0x8301, 6);
    w(0x8302, 7);
    w(0x8303, 8);
    w(0x8100, 0x30);
    check(pr(0x6000) == 8 && pr(0x8000) == 5 && pr(0xA000) == 6 && pr(0xC000) == 7 && pr(0xE000) == 31, "8K PRG mode");
    w(0x8310, 21);
    w(0x8311, 42);
    w(0x8316, 129);
    w(0x8317, 255);
    check(ch(0) == 21 && ch(0x400) == 21 && ch(0x800) == 42 && ch(0x1000) == 129 && ch(0x1800) == 255, "large CHR wiring");
    check(m.ppuMapRead(0x400, p, d, 0) && p == 21 * 0x800 + 0x400, "CHR A10");
    MirrorMode modes[] = {MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_LO, MirrorMode::ONESCREEN_HI};
    for (int i = 0; i < 4; i++)
    {
        w(0x8100, i);
        check(m.getMirrorMode() == modes[i], "mirroring");
    }
    w(0x5102, 0xA7);
    check(m.cpuMapRead(0x5102, p, d) && d == 0xA7, "scratch RAM");
    d = 0xAF;
    check(m.cpuMapRead(0x5000, p, d) && d == 0xAC, "solder pad bits");
    Mapper083 normal(8, 32, false, MirrorMode::HORIZONTAL);
    normal.cpuMapWrite(0x8313, p, 0x55, 0);
    check(normal.ppuMapRead(0xC00, p, d, 0) && p == 0x55 * 0x400, "1K CHR wiring");
}
static void irq()
{
    Mapper083 m(16, 64, true, MirrorMode::VERTICAL);
    uint32_t p = 0;
    auto w = [&](uint16_t a, uint8_t v)
    { m.cpuMapWrite(a, p, v, 0); };
    w(0x8100, 0xC0);
    w(0x8200, 2);
    w(0x8201, 0);
    m.tick();
    check(!m.getIrqFlag(), "IRQ early");
    m.tick();
    check(m.getIrqFlag(), "decrement IRQ");
    w(0x8200, 0xFE);
    check(!m.getIrqFlag(), "IRQ ack");
    w(0x8100, 0x80);
    w(0x8201, 0xFF);
    m.tick();
    check(!m.getIrqFlag(), "increment IRQ early");
    m.tick();
    check(m.getIrqFlag(), "increment IRQ");
    w(0x8200, 2);
    w(0x8100, 0xC0);
    w(0x8318, 0xFF);
    w(0x8201, 0);
    for (int i = 0; i < 10; i++)
        m.tick();
    check(!m.getIrqFlag(), "incorrect M2 clock");
    m.onPpuAddress(0, 0);
    m.onPpuAddress(0x1000, 0);
    m.onPpuAddress(0x1000, 0);
    check(!m.getIrqFlag(), "A12 early");
    std::stringstream saved;
    m.saveState(saved);
    m.onPpuAddress(0, 0);
    m.onPpuAddress(0x1000, 0);
    check(m.getIrqFlag(), "A12 IRQ");
    saved.seekg(0);
    m.loadState(saved);
    check(bool(saved) && !m.getIrqFlag(), "state load");
    m.onPpuAddress(0, 0);
    m.onPpuAddress(0x1000, 0);
    check(m.getIrqFlag(), "restored A12 IRQ");
    auto bad = saved.str();
    bad.resize(10);
    std::stringstream broken(bad);
    m.loadState(broken);
    check(!broken && m.getIrqFlag(), "state rollback");
}
static std::vector<uint8_t> image(bool nes2)
{
    std::vector<uint8_t> r(16 + 16 * 0x4000 + 64 * 0x2000);
    r[0] = 'N';
    r[1] = 'E';
    r[2] = 'S';
    r[3] = 0x1A;
    r[4] = 16;
    r[5] = 64;
    r[6] = 0x31;
    r[7] = nes2 ? 0x58 : 0x50;
    if (nes2)
        r[8] = 0x10;
    return r;
}
static void headers()
{
    auto r = image(false);
    check(FixtureCartridge(r).isValid(), "legacy ROM");
    r = image(true);
    check(FixtureCartridge(r).isValid(), "submapper 1");
    r[8] = 0x20;
    check(!FixtureCartridge(r).isValid(), "unsupported submapper");
    r = image(false);
    r.pop_back();
    check(!FixtureCartridge(r).isValid(), "truncated CHR");
    r = image(false);
    r.resize(16 + 15 * 0x4000);
    check(!FixtureCartridge(r).isValid(), "truncated PRG");
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 83, "ROM rejected");
    unsigned frames = 0;
    for (unsigned step = 0; step < 40000000 && frames < 900; step++)
    {
        nes.step(false);
        if (!nes.isFrameComplete())
            continue;
        nes.clearFrameComplete();
        ++frames;
        if (frames == 300 || frames == 500 || frames == 650)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, true);
        if (frames == 305 || frames == 505 || frames == 655)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, false);
    }
    const auto *px = nes.getPpu().getFrameBuffer();
    std::set<uint32_t> colors(px, px + 256 * 240);
    check(frames == 900 && colors.size() > 2, "no visible video");
    std::stringstream saved;
    check(nes.saveState(saved), "save");
    auto before = saved.str();
    for (int i = 0; i < 10000; i++)
        nes.step(false);
    saved.seekg(0);
    check(nes.loadState(saved), "load");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == before, "state mismatch");
    std::cout << "900 frames, " << colors.size() << " colors, CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        banks();
        irq();
        headers();
        if (argc == 2)
            smoke(argv[1]);
        std::cout << "Mapper 83 tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}