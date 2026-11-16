#include "Core/Cartridge/Mappers/Mapper091.h"
#include "TestFixtures.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
static void mapping()
{
    Mapper091 m(32, 128, MirrorMode::VERTICAL);
    uint32_t mapped = 0;
    uint8_t data = 0;
    auto write = [&](uint16_t addr, uint8_t value)
    { check(m.cpuMapWrite(addr, mapped, value, 0), "register write"); };
    auto prg = [&](uint16_t addr)
    { check(m.cpuMapRead(addr, mapped, data), "PRG read"); return mapped / 0x2000; };
    auto chr = [&](uint16_t addr)
    { check(m.ppuMapRead(addr, mapped, data, 0), "CHR read"); return mapped / 0x800; };
    write(0x7000, 3);
    write(0x7FFD, 5);
    check(prg(0x8000) == 3 && prg(0xA000) == 5 && prg(0xC000) == 14 && prg(0xE000) == 15, "PRG banks");
    for (unsigned i = 0; i < 4; ++i)
        write(0x6FF0 + i, uint8_t(7 + i));
    for (unsigned i = 0; i < 4; ++i)
        check(chr(i * 0x800) == 7 + i, "CHR banks");
    write(0x8000, 7);
    check(prg(0x8000) == 51 && prg(0xC000) == 62 && chr(0) == 263, "outer banks");
    check(!m.ppuMapWrite(0, mapped, 1, 0) && !m.cpuMapRead(0x6000, mapped, data), "ROM and open bus");
    check(m.getMirrorMode() == MirrorMode::VERTICAL, "hardwired mirroring");
}
static void irq()
{
    Mapper091 m(8, 64, MirrorMode::VERTICAL);
    uint32_t p = 0;
    auto edge = [&]
    { m.onPpuAddress(0,0); m.onPpuAddress(0x1000,0); };
    m.cpuMapWrite(0x7007, p, 0, 0);
    for (int i = 0; i < 63; i++)
        edge();
    check(!m.getIrqFlag(), "IRQ early");
    std::stringstream state;
    m.saveState(state);
    edge();
    check(m.getIrqFlag(), "IRQ missing at 64th A12 edge");
    m.cpuMapWrite(0x7006, p, 0, 0);
    check(!m.getIrqFlag(), "IRQ acknowledge");
    state.seekg(0);
    m.loadState(state);
    check(bool(state) && !m.getIrqFlag(), "IRQ state restore");
    m.onPpuAddress(0x1000, 0);
    check(!m.getIrqFlag(), "repeated high A12");
    edge();
    check(m.getIrqFlag(), "restored IRQ timing");
    std::stringstream saved;
    m.saveState(saved);
    auto bad = saved.str();
    bad.resize(8);
    std::stringstream truncated(bad);
    m.loadState(truncated);
    check(!truncated && m.getIrqFlag(), "invalid state changed mapper");
    m.cpuMapWrite(0x7006, p, 0, 0);
    for (int i = 0; i < 65; i++)
        edge();
    check(!m.getIrqFlag(), "disabled IRQ");
}
static void headers()
{
    std::vector<uint8_t> rom(16 + 8 * 0x4000 + 64 * 0x2000);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 8;
    rom[5] = 64;
    rom[6] = 0xB1;
    rom[7] = 0x58;
    check(FixtureCartridge(rom).isValid(), "NES 2.0 image rejected");
    rom[8] = 0x10;
    check(!FixtureCartridge(rom).isValid(), "unsupported submapper accepted");
    rom[8] = 0;
    rom.pop_back();
    check(!FixtureCartridge(rom).isValid(), "truncated image accepted");
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 91, "ROM rejected");
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
    auto expected = state.str();
    for (int i = 0; i < 10000; ++i)
        nes.step(false);
    state.seekg(0);
    check(nes.loadState(state), "load");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == expected, "state mismatch");
    std::cout << "600 frames, " << colors.size() << " colors, CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        mapping();
        irq();
        headers();
        if (argc == 2)
            smoke(argv[1]);
        std::cout << "Mapper 91 tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}