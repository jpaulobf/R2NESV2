#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper071.h"
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

static uint32_t mappedRead(Mapper071 &mapper, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(mapper.cpuMapRead(address, mapped, data), "PRG read rejected");
    return mapped;
}

static void write(Mapper071 &mapper, uint16_t address, uint8_t data)
{
    uint32_t mapped = 0;
    check(mapper.cpuMapWrite(address, mapped, data, 0), "Register write rejected");
    check(mapped == 0xFFFFFFFF, "Register write must not map to ROM");
}

static void testMapper()
{
    Mapper071 mapper(16, 0, MirrorMode::VERTICAL);
    check(mappedRead(mapper, 0x8000) == 0, "Initial switchable bank");
    check(mappedRead(mapper, 0xFFFF) == 0x3FFFF, "Fixed bank end");
    for (uint16_t address : {0xC000, 0xDFFF, 0xE000, 0xFFFF})
    {
        write(mapper, address, 0xFA);
        check(mappedRead(mapper, 0xBFFF) == 10 * 0x4000 + 0x3FFF, "Bank select/mask");
        check(mappedRead(mapper, 0xC000) == 15 * 0x4000, "Fixed bank changed");
    }
    for (uint16_t address : {0x8000, 0x8FFF, 0xA000, 0xBFFF})
    {
        write(mapper, address, 0x10);
        check(mappedRead(mapper, 0x8000) == 10 * 0x4000, "Non-bank write changed bank");
        check(mapper.getMirrorMode() == MirrorMode::VERTICAL, "Legacy fixed mirroring changed");
    }
    write(mapper, 0x9000, 0x10);
    check(mapper.getMirrorMode() == MirrorMode::ONESCREEN_HI, "Legacy Fire Hawk detection");
    std::stringstream state;
    mapper.saveState(state);
    mapper.reset();
    check(mappedRead(mapper, 0x8000) == 0, "Reset bank");
    check(mapper.getMirrorMode() == MirrorMode::VERTICAL, "Reset mirroring");
    mapper.loadState(state);
    check(mappedRead(mapper, 0x8000) == 10 * 0x4000, "Restore bank");
    check(mapper.getMirrorMode() == MirrorMode::ONESCREEN_HI, "Restore mirroring");
    write(mapper, 0x9FFF, 0);
    check(mapper.getMirrorMode() == MirrorMode::ONESCREEN_LO, "Legacy lower nametable");

    uint32_t mapped = 0;
    uint8_t data = 0;
    check(!mapper.cpuMapRead(0x7FFF, mapped, data), "Unexpected PRG RAM");
    check(!mapper.cpuMapWrite(0x7FFF, mapped, 0, 0), "Unexpected PRG RAM write");
    check(mapper.ppuMapRead(0x1FFF, mapped, data, 0) && mapped == 0x1FFF, "CHR read");
    check(mapper.ppuMapWrite(0x1FFF, mapped, 0, 0) && mapped == 0x1FFF, "CHR RAM write");
    check(!mapper.ppuMapRead(0x2000, mapped, data, 0), "Nametable mapped as CHR");
    check(!mapper.ppuMapWrite(0x2000, mapped, 0, 0), "Nametable write mapped as CHR");
    Mapper071 chrRom(8, 1, MirrorMode::HORIZONTAL);
    check(!chrRom.ppuMapWrite(0, mapped, 0, 0), "CHR ROM must be read only");

    for (uint8_t banks : {1, 8, 16})
    {
        Mapper071 sized(banks, 0, MirrorMode::HORIZONTAL);
        write(sized, 0xC000, 0xFF);
        check(mappedRead(sized, 0xBFFF) < banks * 0x4000u, "Bank outside PRG bounds");
    }
    Mapper071 empty(0, 0, MirrorMode::HORIZONTAL);
    write(empty, 0xC000, 0xFF);
    check(!empty.cpuMapRead(0xC000, mapped, data), "Empty PRG mapped");

    Mapper071 fixed(16, 0, MirrorMode::HORIZONTAL, 0, false);
    write(fixed, 0x9000, 0x10);
    check(fixed.getMirrorMode() == MirrorMode::HORIZONTAL, "NES 2.0 fixed mirroring changed");
    Mapper071 fireHawk(16, 0, MirrorMode::VERTICAL, 1, false);
    check(fireHawk.getMirrorMode() == MirrorMode::ONESCREEN_LO, "BF9097 initial mirroring");
    write(fireHawk, 0x8000, 0x10);
    check(fireHawk.getMirrorMode() == MirrorMode::ONESCREEN_HI, "BF9097 $8000 mirroring");
    write(fireHawk, 0x9FFF, 0);
    check(fireHawk.getMirrorMode() == MirrorMode::ONESCREEN_LO, "BF9097 $9FFF mirroring");
    write(fireHawk, 0xC000, 0xFF);
    check(mappedRead(fireHawk, 0x8000) == 7 * 0x4000, "BF9097 three-bit bank select");
}

static void testCartridge(const std::filesystem::path &directory, bool nes2, uint8_t submapper)
{
    // ROM sintética: cada banco contém seu número para identificar o mapeamento.
    std::vector<uint8_t> rom(16 + 16 * 0x4000, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 16;
    rom[6] = 0x71; // Mapper 71, espelhamento vertical.
    rom[7] = nes2 ? 0x48 : 0x40;
    rom[8] = nes2 ? submapper << 4 : 0;
    if (nes2)
        rom[11] = 7; // NES 2.0: 8 KiB de CHR-RAM.
    for (size_t bank = 0; bank < 16; ++bank)
        std::fill(rom.begin() + 16 + bank * 0x4000,
                  rom.begin() + 16 + (bank + 1) * 0x4000, static_cast<uint8_t>(bank));
    // O byte zero no endereço de escrita deve ser ignorado (sem bus conflicts).
    rom[16 + 15 * 0x4000] = 0;
    const auto path = directory / ("mapper071-" + std::to_string(nes2) + "-" + std::to_string(submapper) + ".nes");
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        check(file.good(), "Could not write fixture");
    }
    Cartridge cart(path.string());
    check(cart.isValid(), "Mapper 71 cartridge rejected");
    uint8_t data = 0;
    check(cart.cpuRead(0x8000, data) && data == 0, "Cartridge initial bank");
    check(cart.cpuRead(0xFFFF, data) && data == 15, "Cartridge fixed bank");
    check(cart.cpuWrite(0xC000, 3, 0), "Cartridge register write");
    check(cart.cpuRead(0x8000, data) && data == 3, "Bank switch or bus conflict failure");
    check(cart.cpuRead(0xC000, data) && data == 0, "PRG ROM mutated");
    check(cart.ppuWrite(0x1FFF, 0xA5, 0), "Cartridge CHR RAM write");
    check(cart.ppuRead(0x1FFF, data, 0) && data == 0xA5, "Cartridge CHR RAM read");
    cart.cpuWrite(0x8000, 0x10, 0);
    check(cart.getMirrorMode() == (submapper == 1 ? MirrorMode::ONESCREEN_HI : MirrorMode::VERTICAL),
          "Cartridge $8000 mirroring/submapper");
    cart.cpuWrite(0x9000, 0x10, 0);
    check(cart.getMirrorMode() == (!nes2 || submapper == 1 ? MirrorMode::ONESCREEN_HI : MirrorMode::VERTICAL),
          "Cartridge $9000 mirroring/submapper");
    std::stringstream state;
    cart.getMapper()->saveState(state);
    cart.saveState(state);
    cart.getMapper()->reset();
    cart.ppuWrite(0x1FFF, 0, 0);
    cart.getMapper()->loadState(state);
    cart.loadState(state);
    check(cart.cpuRead(0x8000, data) && data == 3, "Cartridge bank state restore");
    check(cart.ppuRead(0x1FFF, data, 0) && data == 0xA5, "Cartridge CHR state restore");
    check(cart.getMirrorMode() == (!nes2 || submapper == 1 ? MirrorMode::ONESCREEN_HI : MirrorMode::VERTICAL),
          "Cartridge mirror state restore");
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        testMapper();
        testCartridge(argv[1], false, 0);
        testCartridge(argv[1], true, 0);
        testCartridge(argv[1], true, 1);
        std::cout << "Mapper 71 tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
