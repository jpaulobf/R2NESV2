#include "Core/NES.h"
#include "Core/Cartridge/Mappers/Mapper189.h"
#include "Core/Cartridge/Mappers/Mapper187.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

static uint32_t read(Mapper189 &mapper, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(mapper.cpuMapRead(address, mapped, data), "PRG read rejected");
    return mapped;
}

static void write(Mapper189 &mapper, uint16_t address, uint8_t data)
{
    uint32_t mapped = 0;
    check(mapper.cpuMapWrite(address, mapped, data, 0), "Register write rejected");
    check(mapped == 0xFFFFFFFF, "Register mapped to ROM");
}

static void testMapper()
{
    Mapper189 mapper(8, 16, MirrorMode::HORIZONTAL);
    check(read(mapper, 0xFFFF) == 0x7FFF, "Reset vector must use bank zero");
    for (uint16_t address : {0x4020, 0x4132, 0x5FFF, 0x6132, 0x7FFF})
    {
        write(mapper, address, 0x20);
        check(read(mapper, 0x8000) == 0x10000, "Upper nibble bank select");
        check(read(mapper, 0xFFFF) == 0x17FFF, "32 KiB bank continuity");
        write(mapper, address, 0x01);
        check(read(mapper, 0x8000) == 0x8000, "Lower nibble bank select");
        write(mapper, address, 0x12);
        check(read(mapper, 0x8000) == 0x18000, "Nibble OR bank select");
    }
    write(mapper, 0x8000, 0x46);
    write(mapper, 0x8001, 0x00);
    check(read(mapper, 0x8000) == 0x18000 && read(mapper, 0xC000) == 0x1C000,
          "MMC3 PRG registers changed independent 32 KiB bank");
    write(mapper, 0x8000, 2);
    write(mapper, 0x8001, 9);
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(mapper.ppuMapRead(0x1000, mapped, data, 0) && mapped == 9 * 0x400,
          "MMC3 CHR banking failed");
    write(mapper, 0xA000, 0);
    check(mapper.getMirrorMode() == MirrorMode::VERTICAL, "MMC3 mirroring failed");
    write(mapper, 0xC000, 1);
    write(mapper, 0xC001, 0);
    write(mapper, 0xE001, 0);
    mapper.onPpuAddress(0, 0);
    for (int edge = 0; edge < 2; ++edge)
    {
        mapper.onPpuAddress(0, 0);
        mapper.tick(); mapper.tick(); mapper.tick();
        mapper.onPpuAddress(0x1000, 0);
    }
    check(mapper.getIrqFlag(), "MMC3 IRQ failed");
    std::stringstream state;
    mapper.saveState(state);
    mapper.reset();
    check(read(mapper, 0x8000) == 0, "Reset bank failed");
    mapper.loadState(state);
    check(read(mapper, 0x8000) == 0x18000 && mapper.getIrqFlag(), "State restore failed");
    write(mapper, 0xE000, 0);
    check(!mapper.getIrqFlag(), "IRQ acknowledge failed");
    check(!mapper.cpuMapRead(0x6000, mapped, data), "Unexpected PRG RAM");
    check(!mapper.cpuMapWrite(0x4017, mapped, 0, 0), "APU register intercepted");
    write(mapper, 0x4132, 0xFF);
    check(read(mapper, 0xFFFF) < 0x20000, "PRG outside ROM");
}

static void testCartridge(const std::filesystem::path &directory, uint8_t mapperId)
{
    std::vector<uint8_t> rom(16 + 0x20000 + 0x20000, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 8; rom[5] = 16;
    rom[6] = (mapperId & 15) << 4; rom[7] = mapperId & 0xF0;
    for (size_t i = 0; i < 0x20000; ++i) rom[16 + i] = i / 0x8000;
    const auto path = directory / ("mapper" + std::to_string(mapperId) + ".nes");
    { std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char *>(rom.data()), rom.size()); }
    Cartridge cart(path.string());
    check(cart.isValid(), "Cartridge rejected");
    check(mapperId == 189 ? dynamic_cast<Mapper189 *>(cart.getMapper().get()) != nullptr
                          : dynamic_cast<Mapper187 *>(cart.getMapper().get()) != nullptr,
          "Cartridge mapper incorrect");
    if (mapperId == 189)
    {
        uint8_t data = 0;
        check(cart.cpuRead(0xFFFF, data) && data == 0, "Cartridge reset bank incorrect");
        cart.cpuWrite(0x4132, 0x30, 0);
        check(cart.cpuRead(0x8000, data) && data == 3, "Cartridge bank switch failed");
        cart.cpuWrite(0xA000, 0, 0);
        check(cart.getMirrorMode() == MirrorMode::VERTICAL, "Cartridge mirroring failed");
    }
}

int main(int argc, char **argv)
{
    try
    {
        check(argc >= 2, "Pass a directory for generated fixtures and optional game path");
        std::filesystem::create_directories(argv[1]);
        testMapper();
        testCartridge(argv[1], 189);
        testCartridge(argv[1], 187); // Unrelated mapper 187 payloads must stay unchanged.
        if (argc >= 3)
        {
            NES nes;
            nes.insertCartridge(argv[2]);
            check(nes.isCartridgeLoaded(), "Game load failed");
            int frames = 0;
            std::set<uint32_t> colors;
            for (int cycle = 0; cycle < 6000000; ++cycle)
            {
                nes.step();
                if (nes.isFrameComplete())
                {
                    ++frames;
                    nes.clearFrameComplete();
                    const auto pixels = nes.getPpu().getFrameBuffer();
                    colors.insert(pixels, pixels + 256 * 240);
                }
            }
            std::cout << "Game: PC=$" << std::hex << nes.getCpu().pc << std::dec
                      << ", frames=" << frames << ", colors=" << colors.size() << '\n';
            check(colors.size() > 4, "Game did not render a visible screen");
        }
        std::cout << "Mapper 189 tests passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
