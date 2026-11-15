#include "Core/Cartridge/Mappers/Mapper212.h"
#include "TestFixtures.h"
#include "MapperMulticartSmoke.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

static std::vector<uint8_t> makeRom(unsigned prg, unsigned chr, bool nes2)
{
    std::vector<uint8_t> rom(16 + prg * 0x4000 + chr * 0x2000, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = static_cast<uint8_t>(prg);
    rom[5] = static_cast<uint8_t>(chr);
    rom[6] = 0x40;
    rom[7] = static_cast<uint8_t>(0xD0 | (nes2 ? 8 : 0));
    if (nes2 && !chr)
        rom[11] = 7;
    for (unsigned bank = 0; bank < prg; ++bank)
        std::fill_n(rom.begin() + 16 + bank * 0x4000, 0x4000, uint8_t(0x10 + bank));
    for (unsigned bank = 0; bank < chr; ++bank)
        std::fill_n(rom.begin() + 16 + prg * 0x4000 + bank * 0x2000, 0x2000, uint8_t(0x40 + bank));
    return rom;
}

static void banking()
{
    for (unsigned prg : {1, 2, 4, 8})
        for (unsigned chr : {0, 1, 2, 4, 8})
            for (bool nes2 : {false, true})
            {
                FixtureCartridge cart(makeRom(prg, chr, nes2));
                check(cart.isValid() && cart.getMapperId() == 212, "Mapper 212 cartridge rejected");
                check(cart.getMirrorMode() == MirrorMode::VERTICAL, "Reset mirroring is not vertical");
                for (uint16_t upper : {0x8000, 0xA120, 0xC000, 0xFFF0})
                    for (unsigned bits = 0; bits < 16; ++bits)
                    {
                        const uint16_t reg = static_cast<uint16_t>((upper & 0xFFF0) | bits);
                        // Zero em ROM e dados diferentes não podem alterar o endereço escrito.
                        check(cart.cpuWrite(reg, 0, 0), "Address register write rejected");
                        check(cart.cpuWrite(reg, 0xFF, 0), "Repeated address register write rejected");
                        for (uint16_t addr : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                        {
                            const unsigned bank = ((upper & 0x4000) ? ((bits & 6) | ((addr >> 14) & 1)) : bits & 7) & (prg - 1);
                            uint8_t value = 0;
                            check(cart.cpuRead(addr, value) && value == 0x10 + bank, "PRG bank mapping mismatch");
                        }
                        uint8_t value = 0;
                        check(cart.ppuRead(0x1234, value, 0), "CHR read rejected");
                        if (chr)
                            check(value == 0x40 + ((bits & 7) & (chr - 1)), "CHR bank mapping mismatch");
                        check(cart.getMirrorMode() == ((bits & 8) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL),
                              "Mapper-controlled mirroring mismatch");
                        check(cart.ppuWrite(0x1234, 0xA5, 0) == (chr == 0), "CHR ROM/RAM write behavior mismatch");
                    }
                if (!chr)
                {
                    uint8_t value = 0;
                    check(cart.ppuRead(0x1234, value, 0) && value == 0xA5, "CHR RAM contents not retained");
                }
            }
}

static void registersAndState()
{
    Mapper212 mapper(8, 8, MirrorMode::HORIZONTAL);
    uint32_t mapped = 0;
    for (uint16_t addr : {0x6000, 0x600F, 0x6010, 0x601F, 0x7FE0, 0x7FF0})
        for (uint8_t open : {0x00, 0xFF, 0x50})
        {
            uint8_t data = open;
            check(mapper.cpuMapRead(addr, mapped, data) && mapped == 0xFFFFFFFF &&
                      data == ((open & 0x70) | ((addr & 0x10) ? 0 : 0x80)),
                  "Menu register/open bus mismatch");
        }
    uint8_t data = 0;
    check(!mapper.cpuMapRead(0x5FFF, mapped, data) && !mapper.cpuMapWrite(0x7FFF, mapped, 0, 0),
          "Unexpected register outside decoded range");
    for (unsigned latch = 0; latch < 32; ++latch)
    {
        const uint16_t reg = static_cast<uint16_t>(0x8000 | ((latch & 16) ? 0x4000 : 0) | (latch & 15));
        mapper.cpuMapWrite(reg, mapped, 0, 0);
        std::stringstream saved;
        mapper.saveState(saved);
        check(saved.str() == std::string(1, static_cast<char>(latch)), "Mapper latch state mismatch");
        mapper.reset();
        saved.seekg(0);
        mapper.loadState(saved);
        std::stringstream restored;
        mapper.saveState(restored);
        check(bool(saved) && restored.str() == std::string(1, static_cast<char>(latch)), "Mapper latch state not restored");
    }
    std::stringstream before;
    mapper.saveState(before);
    for (const std::string bad : {std::string(), std::string(1, '\x20'), std::string(1, '\xFF')})
    {
        std::stringstream invalid(bad);
        mapper.loadState(invalid);
        std::stringstream after;
        mapper.saveState(after);
        check(invalid.fail() && after.str() == before.str(), "Invalid state changed mapper");
    }
    mapper.reset();
    check(mapper.getMirrorMode() == MirrorMode::VERTICAL, "Reset did not restore latch");
}

static void console()
{
    FixtureNES nes("");
    auto cart = std::make_shared<FixtureCartridge>(makeRom(8, 8, true));
    check(cart->isValid() && nes.insertCartridge(cart), "Mapper 212 console fixture failed");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    bus.cpuWrite(0x0000, 0x70);
    check(bus.cpuRead(0x6000) == 0xF0, "Menu A4 low/open bus mismatch");
    bus.cpuWrite(0x0000, 0x50);
    check(bus.cpuRead(0x6010, true) == 0x50 && bus.cpuRead(0x5000) == 0x50,
          "Read-only menu access changed open bus");
    bus.cpuWrite(0xC00B, 0);
    check(bus.cpuRead(0x8000) == 0x12 && bus.cpuRead(0xC000) == 0x13,
          "Console PRG window mismatch");
    check(ppu.ppuRead(0) == 0x43, "Console CHR window mismatch");
    ppu.ppuWrite(0x2000, 0x51);
    ppu.ppuWrite(0x2800, 0x62);
    check(ppu.ppuRead(0x2400) == 0x51 && ppu.ppuRead(0x2C00) == 0x62,
          "Horizontal nametable mirroring mismatch");
    std::stringstream saved;
    check(nes.saveState(saved), "Console save failed");
    bus.cpuWrite(0x8004, 0xFF);
    check(cart->getMirrorMode() == MirrorMode::VERTICAL && bus.cpuRead(0x8000) == 0x14,
          "Alternate latch not applied");
    saved.seekg(0);
    check(nes.loadState(saved) && cart->getMirrorMode() == MirrorMode::HORIZONTAL &&
              bus.cpuRead(0x8000) == 0x12 && ppu.ppuRead(0) == 0x43,
          "Console state did not restore mapper");
    std::stringstream stable;
    check(nes.saveState(stable), "Console snapshot failed");
    auto corrupt = stable.str();
    corrupt.back() = '\x20';
    std::stringstream bad(corrupt);
    check(!nes.loadState(bad), "Invalid mapper latch accepted in console state");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == stable.str(), "Invalid mapper state was not rolled back");
    nes.reset();
    check(cart->getMirrorMode() == MirrorMode::VERTICAL && bus.cpuRead(0xC000) == 0x10,
          "Console reset did not restore Mapper 212 registers");
}

static void invalidImages()
{
    for (bool nes2 : {false, true})
    {
        for (unsigned prg : {0, 3, 16})
            check(!FixtureCartridge(makeRom(prg, 8, nes2)).isValid(), "Invalid PRG size accepted");
        for (unsigned chr : {3, 16})
            check(!FixtureCartridge(makeRom(8, chr, nes2)).isValid(), "Invalid CHR size accepted");
        for (uint8_t flag : {2, 4, 8})
        {
            auto rom = makeRom(8, 8, nes2);
            rom[6] |= flag;
            check(!FixtureCartridge(rom).isValid(), "Unsupported board flags accepted");
        }
        auto truncated = makeRom(8, 8, nes2);
        truncated.pop_back();
        check(!FixtureCartridge(truncated).isValid(), "Truncated CHR accepted");
        truncated = makeRom(8, 8, nes2);
        truncated.resize(16 + 7 * 0x4000);
        check(!FixtureCartridge(truncated).isValid(), "Truncated PRG accepted");
    }
    for (int field : {8, 10, 11, 14})
    {
        auto rom = makeRom(8, 8, true);
        rom[field] = field == 8 ? 0x10 : 1;
        check(!FixtureCartridge(rom).isValid(), "Unsupported NES 2.0 configuration accepted");
    }
}

int main(int argc, char **argv)
{
    try
    {
        banking();
        registersAndState();
        console();
        invalidImages();
        if (argc > 2)
            throw std::runtime_error("Pass at most one optional multicart ROM");
        if (argc == 2)
            smokeMulticart(argv[1], 212);
        std::cout << "Mapper 212 tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}