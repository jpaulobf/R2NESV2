#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper007.h"
#include "Core/NES.h"
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
    rom[16 + 0x8000 + 2] = 0x12; // Banco 1 difere do 3 no mesmo endereço.
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
    check(cart.cpuWrite(0x8002, 0x13, 0), "Second AND test write rejected");
    check(read(0xFFFF) == (conflicts ? 2 : 3), "Bus conflict used a stale or destination bank");
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

static std::string state(Mapper &mapper)
{
    std::stringstream output;
    mapper.saveState(output);
    return output.str();
}

static uint32_t mappedRead(Mapper007 &mapper, uint16_t address)
{
    uint32_t mapped = 0;
    uint8_t data = 0;
    check(mapper.cpuMapRead(address, mapped, data), "PRG read rejected");
    return mapped;
}

static void testMapper()
{
    for (uint16_t banks : {1, 2, 4, 8, 16, 32})
    {
        Mapper007 mapper(banks, 0, MirrorMode::VERTICAL);
        uint32_t mapped = 0;
        uint8_t data = 0;
        const unsigned bankCount = std::max(1u, unsigned(banks / 2));
        for (unsigned value = 0; value < 256; ++value)
        for (uint16_t reg : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
        {
            check(mapper.cpuMapWrite(reg, mapped, value, 0) && mapped == 0xFFFFFFFF,
                  "Register write rejected or mapped to ROM");
            for (uint16_t address : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
            {
                const unsigned bank = value & (bankCount - 1);
                check(mappedRead(mapper, address) ==
                      ((bank * 0x8000 + (address & 0x7FFF)) & (banks * 0x4000 - 1)),
                      "Incorrect PRG bank, ignored bit or physical mirroring");
            }
            check(mapper.getMirrorMode() == ((value & 0x10) ? MirrorMode::ONESCREEN_HI : MirrorMode::ONESCREEN_LO),
                  "Nametable page not independent of PRG selection");
        }
        for (uint16_t address : {0x0000, 0x4020, 0x6000, 0x7FFF})
        {
            check(!mapper.cpuMapRead(address, mapped, data), "Unexpected PRG RAM");
            check(!mapper.cpuMapWrite(address, mapped, 0, 0), "Unexpected register below $8000");
        }
        for (uint16_t address : {0x0000, 0x0FFF, 0x1000, 0x1FFF})
        {
            check(mapper.ppuMapRead(address, mapped, data, 0) && mapped == address, "CHR RAM read mapping");
            check(mapper.ppuMapWrite(address, mapped, 0xA5, 0) && mapped == address, "CHR RAM write mapping");
        }
        check(!mapper.ppuMapRead(0x2000, mapped, data, 0) &&
              !mapper.ppuMapWrite(0x2000, mapped, 0, 0), "Nametable mapped as CHR");
        mapper.reset();
        check(mappedRead(mapper, 0x8000) == 0 && mapper.getMirrorMode() == MirrorMode::ONESCREEN_LO,
              "Complete initialization did not select bank zero/lower nametable");
    }
    for (uint16_t banks : {0, 3, 6, 33, 64, 256})
    {
        bool rejected = false;
        try { Mapper007 mapper(banks, 0, MirrorMode::HORIZONTAL); }
        catch (const std::invalid_argument &) { rejected = true; }
        check(rejected, "Invalid PRG configuration accepted directly");
    }
    for (const auto config : {std::pair<uint16_t, MirrorMode>{1, MirrorMode::HORIZONTAL},
                             {0, MirrorMode::FOUR_SCREEN}, {0, MirrorMode::ONESCREEN_HI}})
    {
        bool rejected = false;
        try { Mapper007 mapper(8, config.first, config.second); }
        catch (const std::invalid_argument &) { rejected = true; }
        check(rejected, "Invalid CHR/mirroring accepted directly");
    }
}

static void testStates()
{
    for (uint16_t banks : {1, 2, 4, 8, 16, 32})
    {
        Mapper007 mapper(banks, 0, MirrorMode::HORIZONTAL);
        const unsigned count = std::max(1u, unsigned(banks / 2));
        for (unsigned bank = 0; bank < count; ++bank)
        for (uint8_t page : {2, 3}) // Valores históricos do enum: LO e HI.
        {
            const std::string legacy{char(bank), char(page), 0, 0, 0};
            std::stringstream input(legacy);
            mapper.loadState(input);
            check(input.good() && state(mapper) == legacy, "Legacy five-byte state rejected");
            check(mappedRead(mapper, 0x8000) == bank * 0x8000 &&
                  mapper.getMirrorMode() == (page == 2 ? MirrorMode::ONESCREEN_LO : MirrorMode::ONESCREEN_HI),
                  "Valid bank/page state not restored");
        }
        const auto before = state(mapper);
        for (size_t length = 0; length < before.size(); ++length)
        {
            auto bytes = before.substr(0, length);
            if (!bytes.empty()) bytes[0] = 0; // Não pode alterar o banco antes de detectar a falha.
            std::stringstream broken(bytes);
            mapper.loadState(broken);
            check(broken.fail() && state(mapper) == before, "Truncated load changed mapper");
        }
        for (unsigned field = 0; field < 5; ++field)
        {
            auto bytes = before;
            bytes[field] = char(field == 0 ? count : 0xFF);
            std::stringstream broken(bytes);
            mapper.loadState(broken);
            check(broken.fail() && state(mapper) == before, "Corrupt load changed mapper");
        }
        for (uint8_t page : {0, 1, 4})
        {
            std::stringstream broken(std::string{0, char(page), 0, 0, 0});
            mapper.loadState(broken);
            check(broken.fail() && state(mapper) == before, "Non-AxROM mirroring accepted in state");
        }
    }
}

static std::vector<uint8_t> makeRom(unsigned banks, bool nes2 = true, uint8_t submapper = 1)
{
    std::vector<uint8_t> rom(16 + banks * 0x4000, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = banks; rom[6] = 0x70; rom[7] = nes2 ? 8 : 0;
    rom[8] = submapper << 4; rom[11] = nes2 ? 7 : 0;
    for (unsigned bank = 0; bank < banks; ++bank)
        std::fill(rom.begin() + 16 + bank * 0x4000, rom.begin() + 16 + (bank + 1) * 0x4000, uint8_t(bank));
    return rom;
}

static std::filesystem::path saveRom(const std::filesystem::path &directory, const std::vector<uint8_t> &rom)
{
    const auto path = directory / "mapper007-fixture.nes";
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
    check(file.good(), "Could not write fixture");
    return path;
}

static void testImages(const std::filesystem::path &directory)
{
    for (bool nes2 : {false, true})
    for (unsigned banks : {1, 2, 4, 8, 16, 32})
    for (bool exponent : {false, true})
    {
        auto bytes = makeRom(banks, nes2);
        if (exponent)
        {
            if (!nes2) continue;
            unsigned power = 14;
            for (unsigned n = banks; n > 1; n >>= 1) ++power;
            bytes[4] = power << 2; bytes[9] = 0x0F;
        }
        bytes[6] |= 1; // H/V do header não substitui o registrador AxROM.
        Cartridge cart(saveRom(directory, bytes).string());
        check(cart.isValid(), "Valid PRG size or exponent header rejected");
        check(cart.cpuWrite(0xFFFF, 0xFF, 0), "Bank write rejected");
        uint8_t data = 0;
        check(cart.cpuRead(0xFFFF, data) && data == banks - 1, "Uppermost physical PRG bank inaccessible");
        check(cart.cpuRead(0x8000, data) && data == (banks == 1 ? 0 : banks - 2), "32 KiB bank alignment");
        check(cart.getMirrorMode() == MirrorMode::ONESCREEN_HI, "Header overrode mirroring register");
    }
    auto reject = [&](const std::vector<uint8_t> &bytes) {
        Cartridge cart(saveRom(directory, bytes).string());
        check(!cart.isValid() && !cart.getMapper(), "Invalid AxROM image accepted");
    };
    for (bool nes2 : {false, true})
    {
        for (unsigned banks : {0, 3, 6, 64}) reject(makeRom(banks, nes2));
        auto bytes = makeRom(8, nes2); bytes.pop_back(); reject(bytes);
        bytes = makeRom(8, nes2); bytes.resize(16 + 0x10000); reject(bytes);
        bytes = makeRom(8, nes2); bytes[5] = 1; bytes.resize(bytes.size() + 8192); reject(bytes);
        for (uint8_t flag : {2, 4, 8})
        {
            bytes = makeRom(8, nes2); bytes[6] |= flag;
            if (flag == 4) bytes.insert(bytes.begin() + 16, 512, 0);
            reject(bytes);
        }
    }
    for (uint8_t submapper = 3; submapper < 16; ++submapper) reject(makeRom(8, true, submapper));
    for (uint8_t ram : {1, 7, 0x70, 0x77})
    {
        auto bytes = makeRom(8); bytes[10] = ram; reject(bytes);
    }
    for (uint8_t ram : {0, 6, 8, 0x70, 0x77})
    {
        auto bytes = makeRom(8); bytes[11] = ram; reject(bytes);
    }
    auto bytes = makeRom(8); bytes[14] = 1; reject(bytes);
    bytes = makeRom(8); bytes[8] |= 1; reject(bytes); // Mapper 263 não é Mapper 7.
    bytes = makeRom(8); bytes[9] = 0x0F; bytes[4] = (14 << 2) | 1; reject(bytes); // 48 KiB.
    bytes[4] = 0xFF; reject(bytes); // Expoente que excede a memória suportada.
}

static std::string consoleState(NES &nes)
{
    std::stringstream output;
    check(nes.saveState(output), "Console save failed");
    return output.str();
}

static void testConsole(const std::filesystem::path &directory)
{
    NES nes;
    nes.insertCartridge(saveRom(directory, makeRom(32)).string());
    check(nes.isCartridgeLoaded(), "Console insertion failed");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    // Cada página física retém dados próprios ao alternar o bit de CIRAM A10.
    for (unsigned page = 0; page < 2; ++page)
    {
        bus.cpuWrite(0x8000, page << 4);
        for (unsigned offset = 0; offset < 0x400; ++offset)
        {
            ppu.ppuWrite(0x2000 + offset, uint8_t(offset + page * 73));
            for (uint16_t base : {0x2000, 0x2400, 0x2800, 0x2C00})
                check(ppu.ppuRead(base + offset) == uint8_t(offset + page * 73), "Single-screen alias mismatch");
        }
    }
    for (unsigned page = 0; page < 2; ++page)
    {
        bus.cpuWrite(0x8000, page << 4);
        for (unsigned offset = 0; offset < 0x400; ++offset)
            check(ppu.ppuRead(0x2000 + offset) == uint8_t(offset + page * 73), "Page switch lost CIRAM contents");
    }
    for (unsigned address = 0; address < 8192; ++address)
        ppu.ppuWrite(address, uint8_t(address ^ (address >> 8)));
    bus.cpuWrite(0xFFFF, 0x1F);
    check(nes.cpuRead(0x8000) == 30 && nes.cpuRead(0xFFFF) == 31, "Oversized bank not selected");
    bus.cpuWrite(0x6000, 0xA5);
    check(bus.cpuRead(0x6000) == 0xA5 && bus.cpuRead(0x7FFF) == 0xA5, "Absent PRG RAM not open bus");
    const auto before = consoleState(nes);
    const size_t mapperStart = before.size() - 8192 - 5;
    for (unsigned field : {0, 1, 4})
    {
        auto damaged = before; damaged[mapperStart + field] = char(0xFF);
        std::stringstream broken(damaged);
        check(!nes.loadState(broken) && consoleState(nes) == before, "Invalid mapper load did not roll back console");
    }
    for (size_t length : {mapperStart, mapperStart + 1, mapperStart + 4, before.size() - 1})
    {
        std::stringstream broken(before.substr(0, length));
        check(!nes.loadState(broken) && consoleState(nes) == before, "Truncated console load changed state");
    }
    nes.reset();
    check(nes.cpuRead(0x8000) == 0 && bus.cart->getMirrorMode() == MirrorMode::ONESCREEN_LO,
          "Console reset did not initialize AxROM");
    check(ppu.ppuRead(0x1FFF) == uint8_t(0x1FFF ^ 0x1F), "Reset erased cartridge CHR RAM");
    ppu.ppuWrite(0x1FFF, 0);
    std::stringstream restore(before);
    check(nes.loadState(restore) && consoleState(nes) == before, "Console state did not restore bank/CHR/CIRAM");
    for (unsigned address = 0; address < 8192; ++address)
    {
        check(ppu.ppuRead(address) == uint8_t(address ^ (address >> 8)), "CHR RAM persistence mismatch");
        check(ppu.ppuReadSprite(address) == ppu.ppuRead(address), "Sprite CHR mapping differs");
    }
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        testMapper();
        testStates();
        testImages(argv[1]);
        testConsole(argv[1]);
        for (uint8_t submapper : {0, 1, 2})
        {
            testCartridge(argv[1], false, submapper);
            testCartridge(argv[1], true, submapper);
        }
        std::cout << "Mapper 7 tests passed (banks, mirroring, conflicts, CHR RAM, states, console, invalid images)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
