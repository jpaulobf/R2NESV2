#include "Core/Cartridge/Mappers/Mapper051.h"
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

static std::vector<uint8_t> makeRom(unsigned prg = 32, unsigned chr = 0, bool nes2 = false)
{
    std::vector<uint8_t> rom(16 + prg * 0x4000 + chr * 0x2000, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1A;
    rom[4] = uint8_t(prg);
    rom[5] = uint8_t(chr);
    rom[6] = 0x31;
    rom[7] = uint8_t(0x30 | (nes2 ? 8 : 0));
    if (nes2 && !chr)
        rom[11] = 7;
    for (unsigned page = 0; page < prg * 2; ++page)
        std::fill_n(rom.begin() + 16 + page * 0x2000, 0x2000, uint8_t(page));
    return rom;
}

static void banking()
{
    // Mesen's page tables, including the independently selectable $6000 window.
    static const unsigned offsets[4][5] = {
        {0x2F, 0, 1, 0x0E, 0x0F}, {0x23, 0, 1, 2, 3}, {0x2F, 2, 3, 0x0E, 0x0F}, {0x23, 0, 1, 2, 3}};
    for (unsigned prg : {1, 2, 4, 8, 16, 32})
        for (bool nes2 : {false, true})
        {
            FixtureCartridge cart(makeRom(prg, 0, nes2));
            check(cart.isValid() && cart.getMapperId() == 51, "Mapper 51 image rejected");
            check(cart.getMirrorMode() == MirrorMode::VERTICAL, "Initial mirroring mismatch");
            for (unsigned bank = 0; bank < 16; ++bank)
                for (unsigned mode = 0; mode < 4; ++mode)
                {
                    const uint8_t control = uint8_t(((mode & 2) << 3) | ((mode & 1) << 1));
                    check(cart.cpuWrite(0x6000, control, 0), "Mode write failed");
                    check(cart.cpuWrite(0x8000, uint8_t(bank | 0xF0), 0), "Bank write failed");
                    for (unsigned window = 0; window < 5; ++window)
                    {
                        const unsigned page = (((bank << 2) | offsets[mode][window])) % (prg * 2);
                        for (unsigned off : {0u, 0x1FFFu})
                        {
                            uint8_t data = 0xFF;
                            check(cart.cpuRead(uint16_t(0x6000 + window * 0x2000 + off), data) && data == page,
                                  "PRG mapping mismatch");
                        }
                    }
                    check(cart.getMirrorMode() == (mode == 3 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL),
                          "Mirroring mismatch");
                }
            uint8_t data = 0;
            check(!cart.cpuRead(0x5FFF, data) && !cart.cpuWrite(0x5FFF, 0, 0), "Unexpected mapping below $6000");
            check(cart.ppuWrite(0x1234, 0x5A, 0) && cart.ppuRead(0x1234, data, 0) && data == 0x5A, "CHR RAM mismatch");
        }
    FixtureCartridge cart(makeRom());
    cart.cpuWrite(0x6000, 0x02, 0); // mode 1
    cart.cpuWrite(0xC123, 0x15, 0); // changes bank and high mode bit
    check(cart.getMirrorMode() == MirrorMode::HORIZONTAL, "$C000 mode bit ignored");
    cart.cpuWrite(0xE123, 0x04, 0); // bank only, preserves mode
    check(cart.getMirrorMode() == MirrorMode::HORIZONTAL, "$E000 changed mode unexpectedly");
    cart.cpuWrite(0xDFFF, 0x04, 0);
    check(cart.getMirrorMode() == MirrorMode::VERTICAL, "$DFFF mode update ignored");
}

static void states()
{
    Mapper051 mapper(32, 0, MirrorMode::VERTICAL);
    uint32_t mapped = 0;
    for (unsigned bank = 0; bank < 16; ++bank)
        for (unsigned mode = 0; mode < 4; ++mode)
        {
            mapper.cpuMapWrite(0x6000, mapped, uint8_t(((mode & 2) << 3) | ((mode & 1) << 1)), 0);
            mapper.cpuMapWrite(0x8000, mapped, uint8_t(bank), 0);
            std::stringstream saved;
            mapper.saveState(saved);
            mapper.reset();
            saved.seekg(0);
            mapper.loadState(saved);
            std::stringstream actual;
            mapper.saveState(actual);
            check(bool(saved) && actual.str() == std::string({char(bank), char(mode)}), "Mapper state mismatch");
        }
    std::stringstream before;
    mapper.saveState(before);
    for (const std::string invalid : {std::string(), std::string(1, '\0'), std::string("\x10\x00", 2), std::string("\x00\x04", 2)})
    {
        std::stringstream input(invalid);
        mapper.loadState(input);
        std::stringstream after;
        mapper.saveState(after);
        check(input.fail() && after.str() == before.str(), "Invalid mapper state applied");
    }
}

static void console()
{
    FixtureNES nes("");
    auto cart = std::make_shared<FixtureCartridge>(makeRom(32, 0, true));
    check(cart->isValid() && nes.insertCartridge(cart), "Mapper console fixture failed");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    bus.cpuWrite(0x6000, 0x12);
    bus.cpuWrite(0x8000, 5);
    check(bus.cpuRead(0x6000) == 0x37 && bus.cpuRead(0x8000) == 20 && cart->getMirrorMode() == MirrorMode::HORIZONTAL,
          "Console mapper state mismatch");
    ppu.ppuWrite(0x1234, 0xA5);
    std::stringstream saved;
    check(nes.saveState(saved), "Save failed");
    bus.cpuWrite(0x6000, 0);
    bus.cpuWrite(0x8000, 0);
    ppu.ppuWrite(0x1234, 0);
    saved.seekg(0);
    check(nes.loadState(saved) && bus.cpuRead(0x8000) == 20 && ppu.ppuRead(0x1234) == 0xA5 &&
              cart->getMirrorMode() == MirrorMode::HORIZONTAL,
          "Console state restore failed");
    std::stringstream snapshot;
    check(nes.saveState(snapshot), "Snapshot failed");
    auto corrupt = snapshot.str();
    corrupt[corrupt.size() - 0x2000 - 1] = '\x04';
    std::stringstream invalid(corrupt);
    check(!nes.loadState(invalid), "Invalid state accepted");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == snapshot.str(), "Failed state was not rolled back");
    nes.reset();
    check(bus.cpuRead(0x8000) == 0 && bus.cpuRead(0xE000) == 3 && cart->getMirrorMode() == MirrorMode::VERTICAL,
          "Reset mapper state mismatch");
}

static void invalidImages()
{
    for (bool nes2 : {false, true})
    {
        for (unsigned prg : {0, 3, 64})
            check(!FixtureCartridge(makeRom(prg, 0, nes2)).isValid(), "Invalid PRG accepted");
        check(!FixtureCartridge(makeRom(32, 1, nes2)).isValid(), "CHR ROM accepted");
        for (uint8_t flag : {2, 4, 8})
        {
            auto r = makeRom(32, 0, nes2);
            r[6] |= flag;
            check(!FixtureCartridge(r).isValid(), "Unsupported flag accepted");
        }
        auto r = makeRom(32, 0, nes2);
        r.pop_back();
        check(!FixtureCartridge(r).isValid(), "Truncated PRG accepted");
    }
    for (int field : {8, 10, 11, 14})
    {
        auto r = makeRom(32, 0, true);
        r[field] = field == 8 ? 0x10 : 1;
        check(!FixtureCartridge(r).isValid(), "Unsupported NES 2.0 configuration accepted");
    }
}

int main(int argc, char **argv)
{
    try
    {
        banking();
        states();
        console();
        invalidImages();
        if (argc > 2)
            throw std::runtime_error("Pass at most one optional ROM");
        if (argc == 2)
            smokeMulticart(argv[1], 51);
        std::cout << "Mapper 51 tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}