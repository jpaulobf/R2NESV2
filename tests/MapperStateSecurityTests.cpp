#include "Core/Cartridge/Mappers/Mapper023.h"
#include "Core/Cartridge/Mappers/Mapper040.h"
#include "Core/Cartridge/Mappers/Mapper066.h"
#include "Core/Cartridge/Mappers/Mapper071.h"
#include "Core/Cartridge/Mappers/Mapper090.h"
#include "Core/Cartridge/Mappers/Mapper187.h"
#include "Core/Cartridge/Mappers/Mapper189.h"
#include "Core/NES.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static std::string save(Mapper &m)
{
    std::ostringstream out;
    m.saveState(out);
    check(bool(out), "save failed");
    return out.str();
}
static void reject(Mapper &m, const std::string &bytes)
{
    const auto before = save(m);
    std::istringstream input(bytes);
    m.loadState(input);
    check(input.fail(), "invalid mapper state accepted");
    check(save(m) == before, "invalid mapper state changed mapper");
}
static void truncations(Mapper &m)
{
    const auto bytes = save(m);
    check(!bytes.empty(), "unexpected empty state");
    for (size_t n : {size_t(0), size_t(1), bytes.size() / 2, bytes.size() - 1})
        reject(m, bytes.substr(0, n));
    std::istringstream input(bytes);
    m.loadState(input);
    check(bool(input) && save(m) == bytes, "valid mapper state failed round trip");
}
static void write(Mapper &m, uint16_t address, uint8_t value)
{
    uint32_t mapped = 0;
    m.cpuMapWrite(address, mapped, value, 0);
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "fixture directory required");
        Mapper023 m23(8, 8, MirrorMode::VERTICAL);
        write(m23, 0x8000, 9);
        write(m23, 0xB000, 4);
        write(m23, 0xF002, 2);
        for (int i = 0; i < 40; ++i)
            m23.tick();
        truncations(m23);
        auto state = save(m23);
        constexpr size_t mirrors = 2 * sizeof(std::underlying_type_t<MirrorMode>);
        auto bad = state;
        bad[mirrors + 2] = 2;
        reject(m23, bad);
        for (size_t offset : {mirrors + 3, state.size() - 6, state.size() - 5,
                              state.size() - 4, state.size() - 3})
        {
            bad = state;
            bad[offset] = 2;
            reject(m23, bad);
        }
        bad = state;
        bad[mirrors] = 32;
        reject(m23, bad);
        bad = state;
        std::memset(&bad[0], 0xff, sizeof(std::underlying_type_t<MirrorMode>));
        reject(m23, bad);
        bad = state;
        std::memset(&bad[bad.size() - sizeof(int16_t)], 0, sizeof(int16_t));
        reject(m23, bad);
        bad = state;
        bad[mirrors + 4 + 8192 + 16] = 16;
        reject(m23, bad);
        for (unsigned value = 0; value < 256; ++value)
        {
            write(m23, 0xB000, static_cast<uint8_t>(value));
            write(m23, 0xB001, static_cast<uint8_t>(value));
            m23.tick();
            const auto valid = save(m23);
            std::istringstream input(valid);
            m23.loadState(input);
            check(bool(input) && save(m23) == valid, "mapper 23 rejected reachable state");
        }

        Mapper040 m40(8, 0);
        write(m40, 0xE000, 7);
        write(m40, 0xA000, 0);
        for (int i = 0; i < 31; ++i)
            m40.tick();
        truncations(m40);
        state = save(m40);
        bad = state;
        bad[0] = 8;
        reject(m40, bad);
        bad = state;
        bad[1] = 2;
        reject(m40, bad);
        bad = state;
        bad[2] = 2;
        reject(m40, bad);
        bad = state;
        uint32_t timer = 8192;
        std::memcpy(&bad[3], &timer, sizeof(timer));
        reject(m40, bad);

        Mapper066 m66(8, 4, MirrorMode::HORIZONTAL);
        write(m66, 0x8000, 0x32);
        truncations(m66);
        state = save(m66);
        bad = state;
        bad[0] = 4;
        reject(m66, bad);
        bad = state;
        bad[1] = 4;
        reject(m66, bad);

        Mapper071 m71(8, 0, MirrorMode::HORIZONTAL);
        write(m71, 0xC000, 3);
        write(m71, 0x9000, 16);
        truncations(m71);
        state = save(m71);
        bad = state;
        bad[0] = 8;
        reject(m71, bad);
        bad = state;
        std::memset(&bad[1], 0xff, sizeof(std::underlying_type_t<MirrorMode>));
        reject(m71, bad);

        Mapper090 m90(8, 8);
        write(m90, 0x9000, 0x12);
        write(m90, 0xA000, 0x34);
        write(m90, 0xC003, 0);
        truncations(m90);
        state = save(m90);
        bad = state;
        bad[35] = 2;
        reject(m90, bad);
        bad = state;
        bad[25] = 4;
        reject(m90, bad);

        Mapper187 m187(8, 16, MirrorMode::HORIZONTAL), other187(8, 16, MirrorMode::HORIZONTAL);
        write(other187, 0x8000, 6);
        write(other187, 0x8001, 4);
        write(other187, 0x5000, 0x83);
        state = save(other187);
        reject(m187, state.substr(0, state.size() - 1));
        truncations(m187);
        Mapper189 m189(8, 16, MirrorMode::HORIZONTAL), other189(8, 16, MirrorMode::HORIZONTAL);
        write(other189, 0x8000, 6);
        write(other189, 0x8001, 4);
        write(other189, 0x4132, 2);
        state = save(other189);
        reject(m189, state.substr(0, state.size() - 1));
        bad = state;
        bad.back() = 16;
        reject(m189, bad);
        truncations(m189);

        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        std::string rom(16 + 8 * 16384 + 8 * 8192, '\0');
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1a;
        rom[4] = 8;
        rom[5] = 8;
        rom[6] = 0x70;
        rom[7] = 0x10; // mapper 23
        const auto path = dir / "mapper23.nes";
        std::ofstream file(path, std::ios::binary);
        file.write(rom.data(), rom.size());
        file.close();
        NES nes("");
        nes.setCheatDirectory(dir / "no-cheats");
        check(nes.insertCartridge(path.string()), "mapper 23 ROM rejected");
        for (int i = 0; i < 25; ++i)
            nes.step();
        std::ostringstream original;
        check(nes.saveState(original), "console save failed");
        const auto mapperState = save(*nes.getBus().cart->getMapper());
        const auto originalBytes = original.str();
        check(originalBytes.size() >= mapperState.size() &&
                  originalBytes.compare(originalBytes.size() - mapperState.size(), mapperState.size(), mapperState) == 0,
              "mapper payload not found");
        bad = originalBytes;
        bad[bad.size() - mapperState.size() + mirrors + 2] = 2;
        std::istringstream invalidConsole(bad);
        check(!nes.loadState(invalidConsole), "console accepted invalid mapper flag");
        std::ostringstream restored;
        check(nes.saveState(restored) && restored.str() == originalBytes, "console rollback failed");
        std::cout << "Mapper state validation and rollback tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}