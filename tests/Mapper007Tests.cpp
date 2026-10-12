#include "Core/Cartridge/Cartridge.h"
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

static void testCartridge(const std::filesystem::path &directory, bool nes2, uint8_t submapper)
{
    // Four 32 KiB banks. Bank zero has zero at every register address:
    // an erroneous bus conflict prevents both bank and nametable selection.
    std::vector<uint8_t> rom(16 + 4 * 0x8000, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 8;
    rom[6] = 0x70;
    rom[7] = nes2 ? 0x08 : 0;
    // In iNES byte 8 is PRG RAM size, not a submapper.
    rom[8] = submapper << 4;
    if (nes2)
        rom[11] = 7;
    for (size_t bank = 0; bank < 4; ++bank)
        std::fill(rom.begin() + 16 + bank * 0x8000,
                  rom.begin() + 16 + (bank + 1) * 0x8000, static_cast<uint8_t>(bank));
    // Conflict-safe register bytes, and distinct bytes for an AND test.
    for (size_t bank = 0; bank < 4; ++bank)
    {
        rom[16 + bank * 0x8000 + 1] = 0xFF;
        rom[16 + bank * 0x8000 + 2] = 0x11;
    }
    const auto path = directory / ("mapper007-" + std::to_string(nes2) + "-" + std::to_string(submapper) + ".nes");
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        check(file.good(), "Could not write fixture");
    }
    Cartridge cart(path.string());
    check(cart.isValid(), "Mapper 7 cartridge rejected");
    const bool conflicts = nes2 && submapper == 2;
    auto read = [&](uint16_t address) {
        uint8_t data = 0;
        check(cart.cpuRead(address, data), "PRG read rejected");
        return data;
    };
    for (uint16_t address : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
    {
        cart.getMapper()->reset();
        check(read(0xFFFF) == 0, "Initial bank");
        const bool handled = cart.cpuWrite(address, 0x13, 0);
        check(read(0xFFFF) == (conflicts ? 0 : 3), "Incorrect AxROM bus conflict/bank selection");
        check(cart.getMirrorMode() == (conflicts ? MirrorMode::ONESCREEN_LO : MirrorMode::ONESCREEN_HI),
              "Incorrect AxROM bus conflict/nametable selection");
        check(read(0x8000) == (conflicts ? 0 : 3), "32 KiB window start");
        check(read(0xC000) == (conflicts ? 0 : 3), "32 KiB window upper half");
        check(handled, "Register write rejected");
    }
    cart.getMapper()->reset();
    check(cart.cpuWrite(0x8001, 0x13, 0), "Conflict-safe write rejected");
    check(read(0xFFFF) == 3, "Conflict-safe bank select");
    check(cart.getMirrorMode() == MirrorMode::ONESCREEN_HI, "Conflict-safe nametable select");
    check(cart.cpuWrite(0x8002, 0x13, 0), "AND test write rejected");
    check(read(0xFFFF) == (conflicts ? 1 : 3), "Bus conflict must AND the currently mapped ROM byte");
    check(cart.cpuWrite(0x8001, 0x02, 0), "Lower nametable write rejected");
    check(read(0xFFFF) == 2, "Bank select after conflict");
    check(cart.getMirrorMode() == MirrorMode::ONESCREEN_LO, "Lower nametable select");
    check(read(0x8001) == 0xFF && read(0x8002) == 0x11, "Register write mutated PRG ROM");
    uint8_t data = 0;
    check(cart.ppuWrite(0x1FFF, 0xA5, 0), "CHR RAM write rejected");
    check(cart.ppuRead(0x1FFF, data, 0) && data == 0xA5, "CHR RAM readback");
    check(!cart.cpuWrite(0x7FFF, 0x13, 0), "Unexpected register below $8000");
    check(read(0xFFFF) == 2, "Write below $8000 changed bank");
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        for (uint8_t submapper : {0, 1, 2})
        {
            testCartridge(argv[1], false, submapper);
            testCartridge(argv[1], true, submapper);
        }
        std::cout << "Mapper 7 tests passed (iNES and NES 2.0 submappers 0/1/2)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
