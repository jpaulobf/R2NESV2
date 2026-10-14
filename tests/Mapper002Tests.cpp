#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper002.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static uint32_t mappedRead(Mapper002 &mapper, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(mapper.cpuMapRead(address, mapped, data), "PRG read rejected");
    return mapped;
}

static void testMapper()
{
    for (uint16_t banks : {1, 2, 4, 8, 16, 32, 64, 128, 256})
    {
        Mapper002 mapper(banks, 0, MirrorMode::VERTICAL);
        uint32_t mapped = 0;
        uint8_t data = 0;
        for (unsigned value = 0; value < 256; ++value)
        {
            for (uint16_t reg : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
            {
                check(mapper.cpuMapWrite(reg, mapped, value, 0), "Register write rejected");
                check(mapped == 0xFFFFFFFF, "Register write mapped to ROM");
                for (uint16_t address : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                {
                    const uint32_t bank = address < 0xC000 ? value & (banks - 1) : banks - 1;
                    check(mappedRead(mapper, address) == bank * 0x4000 + (address & 0x3FFF),
                          "Incorrect bank/window boundary");
                }
            }
        }
        check(!mapper.cpuMapRead(0x7FFF, mapped, data), "Unexpected PRG RAM");
        check(!mapper.cpuMapWrite(0x7FFF, mapped, 0, 0), "Unexpected register below $8000");
        check(mapper.getMirrorMode() == MirrorMode::VERTICAL, "Register changed mirroring");
        mapper.reset();
        check(mappedRead(mapper, 0x8000) == 0, "Cold reset did not select bank zero");
        check(mapper.ppuMapRead(0x1FFF, mapped, data, 0) && mapped == 0x1FFF, "CHR RAM read");
        check(mapper.ppuMapWrite(0, mapped, 0, 0) && mapped == 0, "CHR RAM start write");
        check(mapper.ppuMapWrite(0x1FFF, mapped, 0, 0) && mapped == 0x1FFF, "CHR RAM end write");
        check(!mapper.ppuMapRead(0x2000, mapped, data, 0), "Nametable mapped as CHR");
        check(!mapper.ppuMapWrite(0x2000, mapped, 0, 0), "Nametable write mapped as CHR");
    }
    for (uint16_t banks : {0, 3, 257, 512})
    {
        bool rejected = false;
        try { Mapper002 invalid(banks, 0, MirrorMode::HORIZONTAL); }
        catch (const std::invalid_argument &) { rejected = true; }
        check(rejected, "Invalid PRG configuration accepted directly");
    }
}

static void testStates()
{
    Mapper002 mapper(4, 0, MirrorMode::VERTICAL);
    uint32_t mapped = 0;
    mapper.cpuMapWrite(0x8000, mapped, 3, 0);
    std::stringstream state;
    mapper.saveState(state);
    check(state.str() == std::string("\x03\x01\x00\x00\x00", 5), "Legacy state layout changed");
    mapper.reset();
    mapper.loadState(state);
    check(state.good() && mappedRead(mapper, 0x8000) == 3 * 0x4000, "Valid state restore");
    // All truncated lengths must leave the previous bank and mirroring intact.
    const auto bytes = state.str();
    for (size_t length = 0; length < bytes.size(); ++length)
    {
        std::stringstream truncated(bytes.substr(0, length));
        mapper.loadState(truncated);
        check(truncated.fail() && mappedRead(mapper, 0x8000) == 3 * 0x4000, "Partial state committed");
    }
    for (unsigned field : {0, 1, 4})
    {
        auto invalid = bytes;
        invalid[field] = static_cast<char>(0xFF);
        std::stringstream corrupt(invalid);
        mapper.loadState(corrupt);
        check(corrupt.fail(), "Invalid state accepted");
        check(mappedRead(mapper, 0x8000) == 3 * 0x4000 && mapper.getMirrorMode() == MirrorMode::VERTICAL,
              "Invalid state mutated mapper");
    }
    std::stringstream wrongMirror(std::string("\x02\x00\x00\x00\x00", 5));
    mapper.loadState(wrongMirror);
    check(wrongMirror.fail() && mappedRead(mapper, 0x8000) == 3 * 0x4000, "Hardware mirroring overwritten");
}

static std::vector<uint8_t> makeRom(unsigned banks, bool nes2, unsigned submapper, bool chrRom = false)
{
    std::vector<uint8_t> rom(16 + banks * 0x4000 + (chrRom ? 0x2000 : 0), 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = static_cast<uint8_t>(banks); rom[5] = chrRom ? 1 : 0;
    rom[6] = 0x21; rom[7] = nes2 ? 8 : 0;
    rom[8] = submapper << 4;
    rom[9] = nes2 ? banks >> 8 : 0;
    rom[11] = nes2 && !chrRom ? 7 : 0;
    for (unsigned bank = 0; bank < banks; ++bank)
    {
        std::fill(rom.begin() + 16 + bank * 0x4000, rom.begin() + 16 + (bank + 1) * 0x4000,
                  static_cast<uint8_t>(bank));
        rom[16 + bank * 0x4000 + 1] = 0xFF; // Conflict-safe address.
        rom[16 + bank * 0x4000 + 2] = 0x01; // AND distinguishes 3 from 1.
    }
    if (chrRom)
        rom[16 + banks * 0x4000] = 0x42;
    return rom;
}

static std::filesystem::path saveRom(const std::filesystem::path &directory, const std::vector<uint8_t> &rom)
{
    // Sequential fixtures are reused only after the previous Cartridge has read them.
    const auto path = directory / "mapper002-fixture.nes";
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
    check(file.good(), "Could not write fixture");
    return path;
}

static unsigned read(Cartridge &cart, uint16_t address)
{
    uint8_t data = 0;
    check(cart.cpuRead(address, data), "Cartridge PRG read rejected");
    return data;
}

static void testCartridges(const std::filesystem::path &directory)
{
    for (bool nes2 : {false, true})
    {
        for (unsigned submapper : {0, 1, 2})
        {
            Cartridge cart(saveRom(directory, makeRom(4, nes2, submapper)).string());
            check(cart.isValid(), "Valid Mapper 2 cartridge rejected");
            const bool conflicts = !nes2 || submapper != 1;
            for (uint16_t address : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
            {
                cart.getMapper()->reset();
                check(cart.cpuWrite(address, 2, 0), "Cartridge register write not handled");
                // Bank zero contains 0; the fixed bank contains 3. Both matter for conflicts.
                check(read(cart, 0x8000) == (conflicts && address < 0xC000 ? 0 : 2), "Submapper conflict policy");
                check(read(cart, 0xFFFF) == 3, "Fixed bank changed");
                check(cart.getMirrorMode() == MirrorMode::VERTICAL, "Mirroring changed");
            }
            check(cart.cpuWrite(0x8001, 3, 0) && read(cart, 0x8000) == 3, "Conflict-safe switch");
            check(cart.cpuWrite(0x8002, 3, 0) && read(cart, 0x8000) == (conflicts ? 1 : 3), "AND conflict");
            check(read(cart, 0x8001) == 0xFF && read(cart, 0x8002) == 1, "PRG ROM modified");
            cart.cpuWrite(0x8001, 2, 0);
            check(cart.cpuWrite(0x8000, 3, 0) && read(cart, 0x8000) == (conflicts ? 2 : 3),
                  "Conflict did not use the previously selected bank");
            uint8_t data = 0;
            check(cart.ppuWrite(0x1FFF, 0xA5, 0), "CHR RAM write rejected");
            std::stringstream state;
            cart.getMapper()->saveState(state); cart.saveState(state);
            cart.getMapper()->reset(); cart.ppuWrite(0x1FFF, 0, 0);
            cart.getMapper()->loadState(state); cart.loadState(state);
            check(state.good() && read(cart, 0x8000) == (conflicts ? 2 : 3), "Bank state restore");
            check(cart.ppuRead(0x1FFF, data, 0) && data == 0xA5, "CHR RAM state restore");
        }
    }
    for (bool exponent : {false, true})
    {
        auto rom = makeRom(256, true, 1);
        if (exponent) { rom[4] = 22 << 2; rom[9] = 0x0F; } // 2^22 bytes.
        Cartridge cart(saveRom(directory, rom).string());
        check(cart.isValid() && read(cart, 0xFFFF) == 255, "4 MiB size/fixed bank");
        check(cart.cpuWrite(0x8000, 255, 0) && read(cart, 0x8000) == 255, "Eight-bit bank register");
        std::stringstream state;
        cart.getMapper()->saveState(state);
        cart.getMapper()->reset(); cart.getMapper()->loadState(state);
        check(state.good() && read(cart, 0x8000) == 255, "4 MiB state bank truncated");
    }
    for (bool nes2 : {false, true})
    {
        auto rom = makeRom(4, nes2, 1, true);
        if (nes2) { rom[5] = 13 << 2; rom[9] = 0xF0; } // CHR exponent: 2^13 bytes.
        Cartridge cart(saveRom(directory, rom).string());
        uint8_t data = 0;
        check(cart.isValid() && cart.ppuRead(0, data, 0) && data == 0x42, "CHR ROM read");
        check(!cart.ppuWrite(0, 0xA5, 0) && !cart.ppuWrite(0x1FFF, 0xA5, 0), "CHR ROM writable");
        check(cart.ppuRead(0, data, 0) && data == 0x42, "CHR ROM mutated");
    }
    auto trainer = makeRom(4, false, 0);
    trainer[6] |= 4;
    trainer.insert(trainer.begin() + 16, 512, 0xAA);
    Cartridge trained(saveRom(directory, trainer).string());
    check(trained.isValid() && read(trained, 0xFFFF) == 3, "Trainer offset");
}

static void testInvalidRoms(const std::filesystem::path &directory)
{
    auto reject = [&](const std::vector<uint8_t> &rom) {
        Cartridge cart(saveRom(directory, rom).string());
        check(!cart.isValid() && !cart.getMapper(), "Invalid/unsupported ROM accepted");
    };
    reject(makeRom(0, false, 0));
    reject(makeRom(3, false, 0));
    reject(makeRom(512, true, 1));
    reject(makeRom(4, true, 3));
    auto rom = makeRom(4, false, 0); rom.resize(16 + 3 * 0x4000); reject(rom);
    rom = makeRom(4, true, 1, true); rom.pop_back(); reject(rom);
    rom = makeRom(4, true, 1); rom[8] |= 1; reject(rom); // Mapper 258 must not alias Mapper 2.
    rom = makeRom(4, true, 1); rom[9] = 15; rom[4] = 255; reject(rom); // uint64 overflow.
    rom = makeRom(4, true, 1); rom[9] = 15; rom[4] = 13 << 2; reject(rom); // Unaligned PRG.
    rom = makeRom(4, true, 1); rom[11] = 6; reject(rom); // Unsupported CHR RAM size.
    rom = makeRom(4, true, 1); rom[10] = 7; reject(rom); // PRG RAM is not mapped.
    rom = makeRom(4, true, 1); rom[11] = 0x70; reject(rom); // CHR NVRAM is not supported.
    rom = makeRom(4, false, 0); rom[6] |= 8; reject(rom); // Four-screen VRAM unavailable.
    rom = makeRom(4, false, 0); rom[6] |= 4; rom.resize(32); reject(rom);
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        testMapper(); testStates(); testCartridges(argv[1]); testInvalidRoms(argv[1]);
        std::cout << "Mapper 2 tests passed (banking, submappers, states, CHR, NES 2.0, invalid images)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
