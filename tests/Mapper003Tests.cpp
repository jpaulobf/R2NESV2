#include "Core/Cartridge/Mappers/Mapper003.h"
#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static std::string state(Mapper &mapper)
{
    std::stringstream output; mapper.saveState(output); return output.str();
}
static std::vector<uint8_t> rom(unsigned prg, unsigned chr, bool nes2, unsigned sub, bool ram = false)
{
    std::vector<uint8_t> bytes(16 + prg * 0x4000 + chr * 0x2000, 0);
    bytes[0] = 'N'; bytes[1] = 'E'; bytes[2] = 'S'; bytes[3] = 0x1A;
    bytes[4] = prg; bytes[5] = chr; bytes[6] = 0x30;
    bytes[7] = nes2 ? 8 : 0; bytes[8] = nes2 ? sub << 4 : 0;
    bytes[10] = ram ? 5 : 0; bytes[11] = nes2 && !chr ? 7 : 0;
    for (unsigned bank = 0; bank < prg; ++bank)
    {
        auto first = bytes.begin() + 16 + bank * 0x4000;
        std::fill(first, first + 0x4000, bank);
        first[0] = 1; first[1] = 0xFF; first[0x3FFF] = 0xF0;
    }
    for (unsigned bank = 0; bank < chr; ++bank)
        std::fill(bytes.begin() + 16 + prg * 0x4000 + bank * 0x2000,
                  bytes.begin() + 16 + prg * 0x4000 + (bank + 1) * 0x2000, 0x40 + bank);
    return bytes;
}
static std::filesystem::path saveRom(const std::filesystem::path &dir, const std::vector<uint8_t> &bytes)
{
    const auto path = dir / "mapper003-fixture.nes";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(output.good(), "Fixture write failed"); return path;
}
static unsigned cpuRead(Cartridge &cart, uint16_t address)
{
    uint8_t value = 0; check(cart.cpuRead(address, value), "CPU read rejected"); return value;
}
static unsigned ppuRead(Cartridge &cart, uint16_t address)
{
    uint8_t value = 0; check(cart.ppuRead(address, value, 0), "PPU read rejected"); return value;
}
static void cartridges(const std::filesystem::path &dir)
{
    for (unsigned prg : {1, 2})
    for (unsigned chr : {0, 1, 2, 4, 8, 16})
    for (bool nes2 : {false, true})
    for (unsigned sub : {0, 1, 2})
    for (bool vertical : {false, true})
    {
        auto bytes = rom(prg, chr, nes2, sub); bytes[6] |= vertical ? 1 : 0;
        Cartridge cart(saveRom(dir, bytes).string());
        check(cart.isValid(), "Supported cartridge rejected");
        if (!chr)
            for (uint16_t address : {0, 0x0FFF, 0x1000, 0x1FFF})
                check(cart.ppuWrite(address, 0x88, 0), "CHR RAM write rejected");
        for (uint16_t reg : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
        {
            const unsigned original = cpuRead(cart, reg);
            for (unsigned value = 0; value < 256; ++value)
            {
                check(cart.cpuWrite(reg, value, 0), "Register write not handled");
                const unsigned effective = nes2 && sub == 1 ? value : value & original;
                const unsigned bank = chr ? effective & (chr - 1) : 0;
                for (uint16_t address : {0, 0x0FFF, 0x1000, 0x1FFF})
                {
                    const unsigned expected = chr ? 0x40 + bank : 0x88;
                    check(ppuRead(cart, address) == expected, "CHR bank/mirroring/conflict incorrect");
                    check(cart.ppuWrite(address, 0x88, 0) == (chr == 0), "CHR write protection incorrect");
                    check(ppuRead(cart, address) == expected, "CHR ROM mutated");
                }
                uint8_t sprite = 0;
                check(cart.ppuReadSprite(0x1000, sprite, 0) && sprite == (chr ? 0x40 + bank : 0x88),
                      "Sprite and background CHR disagree");
                check(cpuRead(cart, reg) == original, "PRG mutated by register write");
                check(cart.getMirrorMode() == (vertical ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL),
                      "Fixed mirroring changed");
            }
        }
        check(cpuRead(cart, 0x8002) == 0 && cpuRead(cart, 0xC002) == prg - 1, "PRG mapping incorrect");
        uint8_t data = 0;
        check(!cart.cpuRead(0x7FFF, data) && !cart.cpuWrite(0x7FFF, 3, 0), "Unexpected PRG RAM");
        check(!cart.ppuRead(0x2000, data, 0) && !cart.ppuWrite(0x2000, 1, 0), "Nametable mapped as CHR");
        cart.cpuWrite(0x8001, 0xFF, 0); cart.getMapper()->reset();
        check(ppuRead(cart, 0) == (chr ? 0x40 : 0x88), "Reset or RAM preservation incorrect");
        if (!chr)
        {
            std::stringstream saved; cart.getMapper()->saveState(saved); cart.saveState(saved);
            cart.ppuWrite(0, 1, 0); cart.getMapper()->loadState(saved); cart.loadState(saved);
            check(saved.good() && ppuRead(cart, 0) == 0x88, "CHR RAM state restore failed");
        }
    }
    // NES 2.0 exponent notation must select the same supported capacities.
    auto bytes = rom(2, 16, true, 1);
    bytes[4] = 15 << 2; bytes[5] = 17 << 2; bytes[9] = 0xFF;
    Cartridge exponent(saveRom(dir, bytes).string());
    check(exponent.isValid() && exponent.cpuWrite(0x8001, 15, 0) && ppuRead(exponent, 0x1FFF) == 0x4F,
          "Exponent sizes or final oversized bank failed");
}
static void ram(const std::filesystem::path &dir)
{
    auto bytes = rom(2, 4, true, 2, true);
    bytes[6] |= 4; bytes.insert(bytes.begin() + 16, 512, 0);
    for (unsigned i = 0; i < 512; ++i) bytes[16 + i] = i & 0xFF;
    Cartridge cart(saveRom(dir, bytes).string()); check(cart.isValid(), "PRG RAM/trainer rejected");
    for (unsigned i = 0; i < 512; ++i)
        check(cpuRead(cart, 0x7000 + i) == (i & 0xFF), "Trainer not installed in PRG RAM");
    for (unsigned i = 0; i < 2048; ++i)
        check(cart.cpuWrite(0x6000 + i, (i ^ (i >> 8)) & 0xFF, 0), "PRG RAM write failed");
    for (unsigned i = 0; i < 8192; ++i)
    {
        const unsigned offset = i & 0x7FF;
        check(cpuRead(cart, 0x6000 + i) == ((offset ^ (offset >> 8)) & 0xFF), "PRG RAM mirror failed");
    }
    cart.cpuWrite(0x7FFF, 0xAB, 0);
    check(cpuRead(cart, 0x67FF) == 0xAB, "PRG RAM final mirror write failed");
    cart.getMapper()->reset(); check(cpuRead(cart, 0x7FFF) == 0xAB, "Reset cleared PRG RAM");
    uint8_t data = 0; check(!cart.cpuRead(0x5FFF, data), "RAM below $6000");
    check(cpuRead(cart, 0x8001) == 0xFF, "Trainer shifted PRG incorrectly");
}
static void states()
{
    for (unsigned chr : {0, 2, 4, 16})
    for (bool ram : {false, true})
    {
        Mapper003 mapper(2, chr, MirrorMode::VERTICAL, ram ? 2048 : 0);
        uint32_t mapped = 0; uint8_t data = 0;
        mapper.cpuMapWrite(0x8000, mapped, 3, 0);
        if (ram) mapper.cpuMapWrite(0x6000, mapped, 0xA5, 0);
        const auto saved = state(mapper);
        check(saved.size() == (ram ? 2053 : 5) && saved.substr(0, 5) == std::string("\x03\x01\0\0\0", 5),
              "Legacy state layout changed");
        mapper.reset(); std::stringstream valid(saved); mapper.loadState(valid);
        check(valid.good() && state(mapper) == saved, "Valid/legacy state rejected");
        for (size_t length = 0; length < saved.size(); ++length)
        {
            auto damaged = saved.substr(0, length); if (!damaged.empty()) damaged[0] = 0;
            std::stringstream partial(damaged); mapper.loadState(partial);
            check(partial.fail() && state(mapper) == saved, "Truncated load changed mapper/RAM");
        }
        for (unsigned field : {0, 1, 4})
        {
            auto damaged = saved; damaged[field] = char(0xFF);
            std::stringstream corrupt(damaged); mapper.loadState(corrupt);
            check(corrupt.fail() && state(mapper) == saved, "Corrupt state accepted or changed mapper");
        }
        mapper.ppuMapRead(0x1FFF, mapped, data, 0);
        check(mapped == (chr ? (3 & (chr - 1)) * 0x2000 : 0) + 0x1FFF,
              "Legacy latch did not mirror physical banks");
        if (ram)
            check(mapper.cpuMapRead(0x7800, mapped, data) && mapped == 0xFFFFFFFF && data == 0xA5,
                  "PRG RAM state restore failed");
    }
    for (unsigned bank : {4, 15})
    {
        Mapper003 mapper(2, 16, MirrorMode::HORIZONTAL); uint32_t mapped = 0;
        mapper.cpuMapWrite(0xFFFF, mapped, bank, 0);
        const auto saved = state(mapper); mapper.reset();
        std::stringstream input(saved); mapper.loadState(input);
        check(input.good() && state(mapper) == saved, "Oversized bank state rejected");
    }
}
static void console(const std::filesystem::path &dir)
{
    for (unsigned chr : {0, 4})
    {
        NES nes; nes.insertCartridge(saveRom(dir, rom(2, chr, true, 1, true)).string());
        check(nes.isCartridgeLoaded(), "Console insert failed"); auto &bus = nes.getBus();
        bus.cpuWrite(0x8000, 3); bus.cpuWrite(0x6000, 0xA5);
        bus.cpuWrite(0x2006, 0); bus.cpuWrite(0x2006, 0); bus.cpuWrite(0x2007, 0x99);
        check(ppuRead(*bus.cart, 0) == (chr ? 0x43 : 0x99), "PPUDATA ROM protection/RAM write failed");
        std::stringstream saved; check(nes.saveState(saved), "Console save failed"); const auto before = saved.str();
        const size_t offset = before.size() - state(*bus.cart->getMapper()).size() - (chr ? 0 : 8192);
        auto damaged = before; damaged[offset] = char(0xFF);
        std::stringstream corrupt(damaged); check(!nes.loadState(corrupt), "Console accepted invalid latch");
        std::stringstream after; check(nes.saveState(after) && after.str() == before, "Console rollback failed");
        nes.reset(); check(ppuRead(*bus.cart, 0) == (chr ? 0x40 : 0x99), "Console reset did not initialize mapper");
        check(nes.cpuRead(0x6000) == 0xA5, "Console reset cleared cartridge RAM");
        bus.cpuWrite(0x6000, 0); if (!chr) bus.cart->ppuWrite(0, 0, 0);
        std::stringstream restore(before); check(nes.loadState(restore), "Console restore failed");
        check(nes.cpuRead(0x6000) == 0xA5 && ppuRead(*bus.cart, 0) == (chr ? 0x43 : 0x99),
              "Console RAM/CHR/bank restore failed");
    }
}
static void invalid(const std::filesystem::path &dir)
{
    auto reject = [&](const std::vector<uint8_t> &bytes) {
        Cartridge cart(saveRom(dir, bytes).string());
        check(!cart.isValid() && !cart.getMapper(), "Invalid image accepted");
    };
    for (unsigned prg : {0, 3, 8}) reject(rom(prg, 4, true, 0));
    for (unsigned chr : {3, 5, 32}) reject(rom(2, chr, true, 0));
    reject(rom(2, 4, true, 3)); reject(rom(2, 4, true, 15));
    for (bool nes2 : {false, true})
    {
        auto bytes = rom(2, 4, nes2, 0); bytes.resize(16 + 0x4000); reject(bytes);
        bytes = rom(2, 4, nes2, 0); bytes.pop_back(); reject(bytes);
        bytes = rom(2, 4, nes2, 0); bytes[6] |= 8; reject(bytes);
        bytes = rom(2, 4, nes2, 0); bytes[6] |= 2; reject(bytes);
        bytes = rom(2, 4, nes2, 0); bytes[6] |= 4; bytes.insert(bytes.begin()+16, 512, 0); reject(bytes);
    }
    for (uint8_t declaration : {1, 4, 6, 7, 0x50, 0x55})
    {
        auto bytes = rom(2, 4, true, 0); bytes[10] = declaration; reject(bytes);
    }
    for (uint8_t declaration : {0, 6, 8, 0x70, 0x77})
    {
        auto bytes = rom(2, 0, true, 0); bytes[11] = declaration; reject(bytes);
    }
    auto bytes = rom(2, 4, true, 0); bytes[11] = 7; reject(bytes);
    bytes = rom(2, 4, true, 0); bytes[14] = 1; reject(bytes); // Misc. ROM/speech unsupported.
    bytes = rom(2, 4, false, 0); bytes[8] = 1; reject(bytes); // Explicit 8 KiB PRG-RAM unsupported.
    bytes = rom(2, 4, true, 0, true); bytes[6] |= 4; bytes.resize(32); reject(bytes);
    bytes = rom(2, 4, true, 0); bytes[8] |= 1; reject(bytes); // Mapper 259 must not alias 3.
    bytes = rom(2, 4, true, 0); bytes[4] = 255; bytes[9] = 15; reject(bytes);
    bytes = rom(2, 4, true, 0); bytes[4] = 13 << 2; bytes[9] = 15; reject(bytes);
    for (const auto config : {std::pair<unsigned, unsigned>{0,4}, {3,4}, {2,3}, {2,32}})
    {
        bool failed = false;
        try { Mapper003 mapper(config.first, config.second, MirrorMode::HORIZONTAL); }
        catch (const std::invalid_argument &) { failed = true; }
        check(failed, "Direct invalid configuration accepted");
    }
    check(!Mapper003::supports(2, 4, MirrorMode::FOUR_SCREEN) &&
          !Mapper003::supports(2, 4, MirrorMode::HORIZONTAL, 8192), "Invalid mirroring/RAM supported");
}
int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        cartridges(argv[1]); ram(argv[1]); states(); console(argv[1]); invalid(argv[1]);
        std::cout << "Mapper 3 tests passed (banks, conflicts, RAM/ROM, states, console, invalid images)\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
