#include "Core/NES.h"
#include "Core/Cartridge/Mappers/Mapper023.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "TestFixtures.h"


using namespace R2NES::Core;
static void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
static void write(Mapper023 &m, uint16_t address, uint8_t data)
{
    uint32_t mapped = 0;
    m.cpuMapWrite(address, mapped, data, 0);
}
static uint32_t prg(Mapper023 &m, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(m.cpuMapRead(address, mapped, data) && mapped != 0xFFFFFFFF, "PRG read not mapped");
    return mapped / 8192;
}
static uint32_t chr(Mapper023 &m, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(m.ppuMapRead(address, mapped, data, 0), "CHR read not mapped");
    return mapped / 1024;
}
static std::string save(Mapper023 &m)
{
    std::ostringstream out;
    m.saveState(out);
    return out.str();
}
static void restore(Mapper023 &m, const std::string &state)
{
    std::istringstream in(state);
    m.loadState(in);
    check(bool(in), "mapper state rejected");
}

static void registers()
{
    // Testa as quatro linhas de endereço, inclusive os espelhos ignorados.
    for (uint8_t variant = 0; variant <= 3; ++variant)
    {
        Mapper023 m(16, 64, MirrorMode::VERTICAL, variant, variant == 3 ? 0 : 8192);
        const uint16_t step = variant == 2 ? 4 : 1;
        const uint16_t ignored = variant == 2 ? 3 : variant == 0 ? 0x10
                                                                 : 0x0C;
        write(m, 0xB000 | ignored, 5);
        write(m, (0xB000 + step) | ignored, 2);
        write(m, (0xB000 + 2 * step) | ignored, 7);
        write(m, (0xB000 + 3 * step) | ignored, 1);
        check(chr(m, 0) == 0x25 && chr(m, 0x400) == 0x17, "CHR register decoding");
        if (variant == 0)
        {
            write(m, 0xC004, 9);
            write(m, 0xC008, 6);
            check(chr(m, 0x800) == 0x90 && chr(m, 0xC00) == 6, "iNES VRC4e decoding");
        }
        write(m, 0x8000, 3);
        write(m, 0xA000, 5);
        check(prg(m, 0x8000) == 3 && prg(m, 0xA000) == 5 && prg(m, 0xC000) == 30 && prg(m, 0xE000) == 31,
              "PRG bank mapping");
        write(m, 0x9000 + 2 * step, 2);
        if (variant == 3)
        {
            check(prg(m, 0x8000) == 3 && prg(m, 0xC000) == 30, "VRC2 enabled VRC4 PRG swap");
            check(m.getMirrorMode() == MirrorMode::VERTICAL, "VRC2 one-screen mirroring enabled");
        }
        else
        {
            check(prg(m, 0x8000) == 30 && prg(m, 0xC000) == 3, "VRC4 PRG swap");
            write(m, 0x9000, 3);
            check(m.getMirrorMode() == MirrorMode::ONESCREEN_HI, "VRC4 one-screen mirroring");
        }
        const auto before = save(m);
        write(m, 0x8000, 1);
        restore(m, before);
        check(save(m) == before, "mapper restore changed state");
        Mapper023 other(16, 64, MirrorMode::VERTICAL, variant == 1 ? 2 : 1);
        const auto unchanged = save(other);
        std::istringstream wrong(before);
        other.loadState(wrong);
        check(wrong.fail() && save(other) == unchanged, "different submapper state accepted");
    }
}

static void irq()
{
    for (uint8_t variant : {uint8_t{0}, uint8_t{1}, uint8_t{2}})
    {
        Mapper023 m(16, 32, MirrorMode::VERTICAL, variant);
        const uint16_t step = variant == 2 ? 4 : 1;
        write(m, 0xF000, 0xE);
        write(m, 0xF000 + step, 0xF);
        write(m, 0xF000 + 2 * step, 7); // Cycle mode, reload FE, repeat after ACK.
        m.tick();
        check(!m.getIrqFlag(), "cycle IRQ early");
        m.tick();
        check(m.getIrqFlag(), "cycle IRQ missing");
        write(m, 0xF000 + 3 * step, 0);
        check(!m.getIrqFlag(), "IRQ acknowledge failed");
        m.tick();
        m.tick();
        check(m.getIrqFlag(), "IRQ repeat missing");
        write(m, 0xF000, 0xF);
        write(m, 0xF000 + step, 0xF);
        write(m, 0xF000 + 2 * step, 2); // 114 CPU cycles until first tick.
        for (int i = 0; i < 113; ++i)
            m.tick();
        check(!m.getIrqFlag(), "scanline IRQ early");
        const auto snapshot = save(m);
        m.tick();
        check(m.getIrqFlag(), "scanline IRQ missing");
        restore(m, snapshot);
        m.tick();
        check(m.getIrqFlag(), "IRQ phase not restored");
    }
    Mapper023 vrc2(16, 32, MirrorMode::VERTICAL, 3, 0);
    write(vrc2, 0xF000, 15);
    write(vrc2, 0xF001, 15);
    write(vrc2, 0xF002, 7);
    for (int i = 0; i < 1000; ++i)
        vrc2.tick();
    check(!vrc2.getIrqFlag(), "VRC2 unexpectedly raised IRQ");
}

static void memory()
{
    Mapper023 vrc2(16, 32, MirrorMode::VERTICAL, 3, 0);
    uint32_t mapped = 0;
    uint8_t data = 0xA6;
    write(vrc2, 0x6ABC, 1);
    check(vrc2.cpuMapRead(0x6000, mapped, data) && data == 0xA7, "VRC2 latch/open bus");
    data = 0x42;
    check(!vrc2.cpuMapRead(0x7000, mapped, data) && data == 0x42, "VRC2 $7000 not open bus");
    const auto state = save(vrc2);
    write(vrc2, 0x6000, 0);
    restore(vrc2, state);
    data = 0xA6;
    check(vrc2.cpuMapRead(0x6FFF, mapped, data) && data == 0xA7, "VRC2 latch restore");
    Mapper023 ram(16, 32, MirrorMode::VERTICAL, 2, 2048, true);
    write(ram, 0x6005, 0xAD);
    data = 0;
    check(ram.cpuMapRead(0x6805, mapped, data) && data == 0xAD, "2 KiB RAM mirroring");
    check(!ram.cpuMapRead(0x7000, mapped, data), "2 KiB RAM responds outside window");
    ram.reset();
    data = 0;
    check(ram.cpuMapRead(0x6005, mapped, data) && data == 0xAD, "reset erased battery RAM");
    auto battery = ram.getBatteryData();
    check(battery.size() == 2048 && battery[5] == 0xAD, "battery data missing");
    Mapper023 loaded(16, 32, MirrorMode::VERTICAL, 2, 2048, true);
    check(loaded.setBatteryData(battery) && loaded.getBatteryData() == battery, "battery restore");
    write(loaded, 0x9008, 0); // Disable WRAM.
    check(!loaded.cpuMapRead(0x6005, mapped, data), "disabled RAM still visible");
}

static void cartridge(const std::filesystem::path &dir)
{
    std::filesystem::create_directories(dir);
    std::vector<uint8_t> rom(16 + 2 * 16384 + 16 * 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = 2;
    rom[5] = 16;
    rom[6] = 0x70;
    rom[7] = 0x18;
    for (int bank = 0; bank < 128; ++bank)
        std::fill_n(rom.begin() + 16 + 2 * 16384 + bank * 1024, 1024, uint8_t(bank));
    const auto path = dir / "vrc2.nes";
    auto saveRom = [&]
    { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char *>(rom.data()), rom.size()); };
    rom[8] = 0x30; // NES 2.0, mapper 23.3 (VRC2b).
    saveRom();
    FixtureNES nes("");
    nes.setCheatDirectory("");
    check(nes.insertCartridge(path.string()), "VRC2b NES 2.0 ROM rejected");
    auto &bus = nes.getBus();
    bus.cpuWrite(0x6000, 1);
    bus.cpuWrite(0x20, 0xA6);
    check(bus.cpuRead(0x6FFF) == 0xA7, "VRC2 latch not wired through CPU bus");
    bus.cpuWrite(0x20, 0xA6);
    check(bus.cpuRead(0x7000) == 0xA6, "VRC2 upper window not open bus");
    std::ostringstream snapshot;
    check(nes.saveState(snapshot), "VRC2 console save");
    bus.cpuWrite(0x6000, 0);
    std::istringstream restoreState(snapshot.str());
    check(nes.loadState(restoreState), "VRC2 console load");
    bus.cpuWrite(0x20, 0xA6);
    check(bus.cpuRead(0x6000) == 0xA7, "VRC2 console latch restore");
    for (uint8_t variant : {uint8_t{0}, uint8_t{1}, uint8_t{2}})
    {
        rom[7] = variant == 0 ? 0x10 : 0x18;
        rom[8] = variant << 4;
        saveRom();
        FixtureNES console("");
        console.setCheatDirectory("");
        check(console.insertCartridge(path.string()), "VRC4 ROM rejected");
        console.getBus().cpuWrite(0xB000, 3);
        console.getBus().cpuWrite(0xB004, 4);
        uint8_t chrByte = 0;
        check(console.getBus().cart->ppuRead(0, chrByte, 0) && chrByte == (variant == 1 ? 4 : 67),
              "cartridge submapper address lines not selected");
    }
    rom[7] = 0x18;
    rom[8] = 0x40;
    saveRom();
    FixtureCartridge unsupported(path.string());
    check(!unsupported.isValid(), "unknown submapper accepted");
    rom[8] = 0x30;
    rom[10] = 7;
    saveRom();
    FixtureCartridge invalidRam(path.string());
    check(!invalidRam.isValid(), "VRC2 with unsupported RAM accepted");

    // Confere o caminho completo de persistência em arquivo, inclusive reset.
    rom[7] = 0x10;
    rom[8] = 0;
    rom[10] = 0;
    rom[6] = 0x72;
    saveRom();
    const auto batteryDir = dir / "battery";
    {
        FixtureNES writer(batteryDir);
        writer.setCheatDirectory("");
        check(writer.insertCartridge(path.string()), "battery ROM rejected");
        writer.getBus().cpuWrite(0x6005, 0xB4);
        writer.reset();
        check(writer.getBus().cpuRead(0x6005) == 0xB4, "reset erased battery RAM");
    }
    {
        FixtureNES reader(batteryDir);
        reader.setCheatDirectory("");
        check(reader.insertCartridge(path.string()), "battery ROM reload rejected");
        check(reader.getBus().cpuRead(0x6005) == 0xB4, "battery file not restored");
    }
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "fixture directory required");
        registers();
        irq();
        memory();
        cartridge(argv[1]);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "Mapper 23 tests passed\n";
}