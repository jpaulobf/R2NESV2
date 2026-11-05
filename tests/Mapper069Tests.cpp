#include "Core/Cartridge/Mappers/Mapper069.h"
#include "Core/Cartridge/Cartridge.h"
#include "Core/Memory/VRAM/VRAM.h"
#include "Core/NES.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <vector>
using namespace R2NES::Core;
static void check(bool x, const char *s)
{
    if (!x)
        throw std::runtime_error(s);
}
static void reg(Mapper069 &m, unsigned r, unsigned v)
{
    uint32_t a;
    m.cpuMapWrite(0x8000, a, r, 0);
    m.cpuMapWrite(0xa000, a, v, 0);
}
static void soundReg(Mapper069 &m, uint8_t reg, uint8_t value)
{
    uint32_t a;
    m.cpuMapWrite(0xc000, a, reg, 0);
    m.cpuMapWrite(0xe000, a, value, 0);
}
static void soundTests()
{
    Mapper069 m(8, 8, false, false, MirrorMode::HORIZONTAL);
    check(m.expansionAudio() == 0, "audio reset must be silent");
    soundReg(m, 7, 0x3e);
    soundReg(m, 8, 15);
    soundReg(m, 0, 2);
    check(m.expansionAudio() == 0, "tone initial phase");
    for (int i = 0; i < 31; i++)
        m.tick();
    check(m.expansionAudio() == 0, "tone toggled early");
    m.tick();
    check(m.expansionAudio() > 0.3f, "tone did not toggle");
    uint32_t a;
    m.cpuMapWrite(0xc000, a, 0x18, 0);
    m.cpuMapWrite(0xe000, a, 0, 0);
    check(m.expansionAudio() > 0.3f, "disabled register select wrote data");
    std::stringstream snapshot;
    m.saveState(snapshot);
    std::vector<float> samples;
    for (int i = 0; i < 500; i++)
    {
        m.tick();
        samples.push_back(m.expansionAudio());
    }
    m.loadState(snapshot);
    check(bool(snapshot), "audio state rejected");
    for (float v : samples)
    {
        m.tick();
        check(m.expansionAudio() == v, "audio phase restore");
    }
    m.reset();
    soundReg(m, 7, 0x3f);
    soundReg(m, 8, 16);
    soundReg(m, 11, 1);
    soundReg(m, 13, 4);
    check(m.expansionAudio() == 0, "ascending envelope start");
    for (int i = 0; i < 16 * 31; i++)
        m.tick();
    check(m.expansionAudio() > 0.3f, "ascending envelope peak");
    for (int i = 0; i < 16; i++)
        m.tick();
    check(m.expansionAudio() == 0, "one-shot envelope did not stop at zero");
    m.reset();
    soundReg(m, 7, 0x37);
    soundReg(m, 8, 15);
    const float initial = m.expansionAudio();
    bool changed = false;
    for (int i = 0; i < 4096; i++)
    {
        m.tick();
        if (m.expansionAudio() != initial)
            changed = true;
    }
    check(changed, "noise output did not change");
}
int main(int argc, char **argv)
{
    try
    {
        soundTests();
        check(argc > 1, "directory missing");
        std::filesystem::create_directories(argv[1]);
        Mapper069 m(8, 8, true, true, MirrorMode::HORIZONTAL);
        uint32_t a = 0;
        uint8_t d = 0;
        reg(m, 9, 5);
        check(m.cpuMapRead(0x8123, a, d) && a == 5 * 8192 + 0x123, "PRG bank");
        check(m.cpuMapRead(0xffff, a, d) && a == 16 * 8192 - 1, "fixed bank");
        reg(m, 8, 0x42);
        check(!m.cpuMapRead(0x6000, a, d), "disabled RAM open bus");
        reg(m, 8, 0xc0);
        check(m.cpuMapWrite(0x6000, a, 0x5a, 0) && m.cpuMapRead(0x6000, a, d) && d == 0x5a, "RAM");
        check(m.getBatteryData().size() == 8192 && m.getBatteryData()[0] == 0x5a, "battery");
        reg(m, 3, 47);
        check(m.ppuMapRead(0xc01, a, d, 0) && a == 47 * 1024 + 1, "CHR bank");
        check(!m.ppuMapWrite(0xc01, a, 0, 0), "CHR ROM write");
        const MirrorMode modes[] = {MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_LO, MirrorMode::ONESCREEN_HI};
        for (unsigned i = 0; i < 4; ++i)
        {
            reg(m, 12, i);
            check(m.getMirrorMode() == modes[i], "mirroring");
        }
        reg(m, 14, 1);
        reg(m, 15, 0);
        reg(m, 13, 0x81);
        m.tick();
        check(!m.getIrqFlag(), "IRQ early");
        m.tick();
        check(m.getIrqFlag(), "IRQ underflow");
        std::stringstream state;
        m.saveState(state);
        reg(m, 13, 0);
        check(!m.getIrqFlag(), "IRQ ack");
        m.loadState(state);
        check(bool(state) && m.getIrqFlag(), "IRQ state restore");
        reg(m, 13, 0x80);
        for (int i = 0; i < 3; i++)
            m.tick();
        check(!m.getIrqFlag(), "IRQ disabled");
        std::vector<uint8_t> rom(16 + 8 * 16384 + 8 * 8192);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1a;
        rom[4] = 8;
        rom[5] = 8;
        rom[6] = 0x50;
        rom[7] = 0x40;
        auto path = std::filesystem::path(argv[1]) / "mapper69.nes";
        {
            std::ofstream f(path, std::ios::binary);
            f.write((char *)rom.data(), rom.size());
        }
        Cartridge cart(path.string());
        check(cart.isValid(), "valid cartridge rejected");
        // The cartridge must forward the mapper's live mirroring to the PPU.
        for (unsigned i = 0; i < 4; ++i)
        {
            check(cart.cpuWrite(0x8000, 12, 0) && cart.cpuWrite(0xa000, i, 0), "mirroring register write");
            check(cart.getMirrorMode() == modes[i], "cartridge did not forward dynamic mirroring");
            VRAM vram;
            vram.write(0x2000, 0x11, cart.getMirrorMode());
            vram.write(0x2400, 0x22, cart.getMirrorMode());
            vram.write(0x2800, 0x33, cart.getMirrorMode());
            vram.write(0x2c00, 0x44, cart.getMirrorMode());
            const uint8_t expected[4][4] = {{0x33, 0x44, 0x33, 0x44}, {0x22, 0x22, 0x44, 0x44}, {0x44, 0x44, 0x44, 0x44}, {0x44, 0x44, 0x44, 0x44}};
            for (unsigned n = 0; n < 4; ++n)
                check(vram.read(0x2000 + n * 0x400, cart.getMirrorMode()) == expected[i][n], "nametable mapping");
        }
        {
            NES nes;
            check(nes.insertCartridge(path.string()), "console cartridge rejected");
            auto &bus = nes.getBus();
            bus.cpuWrite(0xc000, 7);
            bus.cpuWrite(0xe000, 0x3f);
            bus.cpuWrite(0xc000, 8);
            bus.cpuWrite(0xe000, 15);
            for (int i = 0; i < 10000; ++i)
                nes.step();
            bool audible = false;
            while (nes.getApu().hasSamples())
                if (nes.getApu().getOutputSample() > 0.001f)
                    audible = true;
            check(audible, "expansion audio missing from APU output");
            std::ostringstream state;
            check(nes.saveState(state), "console save failed");
            std::string legacy = state.str();
            check(legacy.size() > 40, "console state too short");
            legacy.resize(legacy.size() - 40);
            legacy[0] = 'L';
            std::istringstream oldState(legacy);
            check(nes.loadState(oldState), "legacy mapper state rejected");
            std::istringstream restored(state.str());
            check(nes.loadState(restored), "new mapper state rejected");
        }
        rom.pop_back();
        {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            f.write((char *)rom.data(), rom.size());
        }
        Cartridge invalid(path.string());
        check(!invalid.isValid(), "truncated cartridge accepted");
        std::cout << "Mapper 69 tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}