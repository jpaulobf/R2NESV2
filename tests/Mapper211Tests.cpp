#include "Core/Cartridge/Mappers/Mapper211.h"
#include "TestFixtures.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static std::vector<uint8_t> image(bool nes2 = false)
{
    std::vector<uint8_t> r(16 + 16 * 0x4000 + 32 * 0x2000);
    r[0] = 'N';
    r[1] = 'E';
    r[2] = 'S';
    r[3] = 0x1A;
    r[4] = 16;
    r[5] = 32;
    r[6] = 0x30;
    r[7] = uint8_t(0xD0 | (nes2 ? 8 : 0));
    for (unsigned page = 0; page < 256; ++page)
        std::fill_n(r.begin() + 16 + 16 * 0x4000 + page * 1024, 1024, uint8_t(page));
    return r;
}
static void nametables()
{
    FixtureNES nes("");
    auto cart = std::make_shared<FixtureCartridge>(image());
    check(cart->isValid() && nes.insertCartridge(cart), "Mapper 211 cartridge rejected");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    auto write = [&](uint16_t a, uint8_t v)
    { bus.cpuWrite(a, v); };
    write(0xD001, 0);
    ppu.ppuWrite(0x2000, 0x12);
    ppu.ppuWrite(0x2400, 0x34);
    check(ppu.ppuRead(0x2800) == 0x12 && ppu.ppuRead(0x2C00) == 0x34, "Default vertical mirroring");
    write(0xD001, 1);
    check(ppu.ppuRead(0x2400) == 0x12 && ppu.ppuRead(0x2800) == 0x34, "Horizontal mirroring");
    write(0xB000, 1);
    write(0xB001, 0);
    write(0xB002, 1);
    write(0xB003, 0);
    write(0xD001, 8);
    check(ppu.ppuRead(0x2000) == 0x34 && ppu.ppuRead(0x2400) == 0x12 && ppu.ppuRead(0x2800) == 0x34 && ppu.ppuRead(0x2C00) == 0x12, "Extended CIRAM mapping");
    check(ppu.ppuRead(0x3000) == 0x34 && ppu.ppuRead(0x3400) == 0x12, "Mirrored nametables");
    write(0xB000, 0x82);
    write(0xB001, 0x03);
    write(0xB002, 0x84);
    write(0xB003, 0x05);
    write(0xB004, 0);
    write(0xB005, 0);
    write(0xB006, 0);
    write(0xB007, 0);
    write(0xD002, 0);
    write(0xD000, 0x20);
    check(ppu.ppuRead(0x2000) == 0x82 && ppu.ppuRead(0x2800) == 0x84, "Selected CHR ROM nametables");
    check(ppu.ppuRead(0x2400) == 0x34 && ppu.ppuRead(0x2C00) == 0x34, "Selected CIRAM nametables");
    write(0xD002, 0x80);
    check(ppu.ppuRead(0x2400) == 3 && ppu.ppuRead(0x2C00) == 5, "Compare bit did not switch ROM selection");
    write(0xD000, 0x60);
    check(ppu.ppuRead(0x2000) == 0x82 && ppu.ppuRead(0x2400) == 3, "All ROM nametables");
    ppu.ppuWrite(0x2400, 0x66);
    check(ppu.ppuRead(0x2400) == 3, "ROM nametable not read-only");
    write(0xD000, 0);
    write(0xD001, 8);
    check(ppu.ppuRead(0x2400) == 0x66, "CIRAM write under ROM mapping");
    std::stringstream saved;
    check(nes.saveState(saved), "Save failed");
    write(0xB001, 0);
    ppu.ppuWrite(0x2400, 0);
    write(0xD001, 0);
    saved.seekg(0);
    check(nes.loadState(saved) && ppu.ppuRead(0x2400) == 0x66 && ppu.ppuRead(0x2000) == 0x12, "Nametable state restore");
    std::stringstream stable;
    check(nes.saveState(stable), "Snapshot failed");
    auto damaged = stable.str();
    damaged.resize(damaged.size() - 10);
    std::stringstream bad(damaged);
    check(!nes.loadState(bad), "Truncated state accepted");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == stable.str(), "Failed state did not roll back");
}
static void latches()
{
    Mapper211 m(16, 32);
    uint32_t p = 0;
    uint8_t v = 0;
    auto w = [&](uint16_t a, uint8_t d)
    { check(m.cpuMapWrite(a, p, d, 0), "Register write failed"); };
    auto c = [&](uint16_t a)
    { check(m.ppuMapRead(a,p,v,0), "CHR read failed"); return p/0x400; };
    w(0xD000, 8);
    w(0xD003, 0xA0);
    w(0x9000, 1);
    w(0x9002, 2);
    w(0x9004, 3);
    w(0x9006, 4);
    check(c(0) == 4 && c(0x1000) == 12, "Initial latch banks");
    m.onPpuAddress(0x0FE8, 0);
    m.onPpuAddress(0x1FE8, 0);
    check(c(0) == 8 && c(0x1000) == 16, "FE latch banks");
    m.onPpuAddress(0x0FD8, 0);
    m.onPpuAddress(0x1FD8, 0);
    check(c(0) == 4 && c(0x1000) == 12, "FD latch banks");
    m.onPpuAddress(0x0FE8, 0);
    std::stringstream state;
    m.saveState(state);
    m.reset();
    state.seekg(0);
    m.loadState(state);
    check(bool(state) && c(0) == 8, "Latch state restore");
    check(!m.cpuMapWrite(0x9800, p, 0, 0), "Undecoded register address accepted");
}
static void invalidImages()
{
    for (bool n : {false, true})
    {
        auto r = image(n);
        r.pop_back();
        check(!FixtureCartridge(r).isValid(), "Truncated CHR accepted");
        r = image(n);
        r.resize(16 + 15 * 0x4000);
        check(!FixtureCartridge(r).isValid(), "Truncated PRG accepted");
        for (uint8_t flag : {2, 4, 8})
        {
            r = image(n);
            r[6] |= flag;
            check(!FixtureCartridge(r).isValid(), "Unsupported board flags");
        }
    }
    for (int field : {8, 10, 11, 14})
    {
        auto r = image(true);
        r[field] = field == 8 ? 0x10 : 1;
        check(!FixtureCartridge(r).isValid(), "Unsupported NES2 memory");
    }
}
static void smoke(const char *path)
{
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path) && nes.getBus().cart->getMapperId() == 211, "Real ROM rejected");
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
    const auto *px = nes.getPpu().getFrameBuffer();
    std::set<uint32_t> colors(px, px + 256 * 240);
    check(frames == 600 && colors.size() > 2, "ROM did not produce visible video");
    std::stringstream s;
    check(nes.saveState(s), "Real ROM save failed");
    auto expected = s.str();
    for (int i = 0; i < 10000; i++)
        nes.step(false);
    s.seekg(0);
    check(nes.loadState(s), "Real ROM restore failed");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == expected, "Real ROM state mismatch");
    std::cout << "Mapper 211: 600 frames, " << colors.size() << " colors, state restored; CRC " << nes.getBus().cart->getRomHash() << '\n';
}
int main(int argc, char **argv)
{
    try
    {
        nametables();
        latches();
        invalidImages();
        if (argc > 2)
            throw std::runtime_error("One optional ROM expected");
        if (argc == 2)
            smoke(argv[1]);
        std::cout << "Mapper 211 tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}