#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "TestFixtures.h"

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
template <class T>
static std::string component(T &device)
{
    std::ostringstream out;
    device.saveState(out);
    check(bool(out), "component save failed");
    return out.str();
}
static std::string snapshot(NES &nes)
{
    std::ostringstream out;
    check(nes.saveState(out), "save failed");
    return out.str();
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "fixture directory required");
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        const auto path = dir / "hostile-state.nes";
        std::vector<uint8_t> rom(16 + 32768 + 8192, 0xea);
        std::fill_n(rom.begin(), 16, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1a;
        rom[4] = 2;
        rom[5] = 1;
        rom[6] = 0x40;
        rom[16] = 0x4c;
        rom[17] = 0;
        rom[18] = 0x80;
        for (size_t p : {size_t(0x7ffa), size_t(0x7ffc), size_t(0x7ffe)})
        {
            rom[16 + p] = 0;
            rom[16 + p + 1] = 0x80;
        }
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        check(bool(file), "fixture write failed");
        file.close();
        FixtureNES nes("");
        nes.setCheatDirectory(dir / "no-cheats");
        check(nes.insertCartridge(path.string()), "fixture ROM rejected");
        nes.reset();
        nes.getPpu().setUnlimitedSprites(true);
        for (int i = 0; i < 30000; ++i)
            nes.step();
        nes.getPpu().cpuWrite(0x2001, 0x18);
        for (int i = 0; i < 1000; ++i)
            nes.step();
        const auto base = snapshot(nes);
        const size_t cpu = 4 + 8 + 2 + 2 * sizeof(uint32_t) + 1 + 5;
        const size_t ppu = cpu + component(nes.getCpu()).size() + 2048;
        const size_t apu = ppu + component(nes.getPpu()).size();
        const size_t mapper = apu + component(nes.getApu()).size();
        check(mapper < base.size(), "invalid fixture layout");
        const std::pair<size_t, size_t> regions[] = {
            {cpu, ppu - 2048}, {ppu + 2048, std::min(apu, ppu + 3900)}, {apu, mapper}, {mapper, std::min(base.size(), mapper + 40)}};
        uint32_t seed = 0x8c72e541;
        const auto random = [&seed]()
        { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
        unsigned accepted = 0, rejected = 0;
        for (unsigned iteration = 0; iteration < 1200; ++iteration)
        {
            std::istringstream reset(base);
            check(nes.loadState(reset), "baseline restore failed");
            auto inputBytes = base;
            const auto region = regions[iteration % 4];
            for (unsigned n = 0, count = 1 + random() % 3; n < count; ++n)
            {
                const size_t offset = region.first + random() % (region.second - region.first);
                inputBytes[offset] = static_cast<char>(random());
            }
            std::istringstream input(inputBytes);
            if (nes.loadState(input))
            {
                ++accepted;
                for (int step = 0; step < 256; ++step)
                    nes.step();
            }
            else
            {
                ++rejected;
                check(snapshot(nes) == base, "rejected mutation changed console");
            }
        }
        check(accepted != 0 && rejected != 0, "mutation corpus did not exercise both outcomes");
        std::cout << "Hostile state mutations: " << accepted << " accepted, " << rejected << " rejected\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}