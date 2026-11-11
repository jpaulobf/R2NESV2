#include "Core/NES.h"
#include <zlib.h>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "TestFixtures.h"

struct Result
{
    unsigned address, baseline;
    std::string page, name;
};

int main(int argc, char **argv)
{
    using namespace R2NES::Core;
    if (argc != 3)
    {
        std::cerr << "Usage: accuracycoin-runner ROM baseline.tsv\n";
        return 2;
    }
    std::ifstream rom(argv[1], std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(rom)), {});
    if (bytes.size() != 40976 || crc32(0, bytes.data(), static_cast<uInt>(bytes.size())) != 0x52EB96BC)
    {
        std::cerr << "Unexpected AccuracyCoin ROM; this baseline requires CRC32 52EB96BC.\n";
        return 2;
    }
    std::ifstream baseline(argv[2]);
    std::vector<Result> expected;
    std::string line;
    while (std::getline(baseline, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        Result entry{};
        std::istringstream row(line);
        std::string address, value;
        if (!std::getline(row, address, '\t') || !std::getline(row, value, '\t') ||
            !std::getline(row, entry.page, '\t') || !std::getline(row, entry.name))
            return 2;
        try
        {
            entry.address = std::stoul(address, nullptr, 16);
            entry.baseline = std::stoul(value, nullptr, 16);
        }
        catch (...)
        {
            return 2;
        }
        if (entry.address < 0x400 || entry.address > 0x498 || entry.baseline > 255)
            return 2;
        expected.push_back(entry);
    }
    if (expected.size() != 147)
    {
        std::cerr << "Invalid baseline.\n";
        return 2;
    }

    FixtureNES nes("");
    nes.setCheatDirectory("");
    if (!nes.insertCartridge(argv[1]))
        return 2;
    nes.getBus().cheats.clear();
    // Pressione Start na tela inicial; mantenha os mesmos instantes da linha de base.
    constexpr uint64_t totalCycles = 150000000;
    for (uint64_t tick = 0; tick < totalCycles; ++tick)
    {
        if (tick == 3000000)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, true);
        if (tick == 3300000)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_START, false);
        nes.step(false);
    }
    // Contadores são preenchidos enquanto a própria ROM desenha a tabela final.
    if (nes.cpuRead(0x35) != 0 || nes.cpuRead(0x37) != 147)
    {
        std::cerr << "AccuracyCoin did not complete within the fixed cycle budget.\n";
        return 2;
    }
    unsigned passed = 0, failed = 0, improved = 0, regressed = 0;
    std::cout << "Page\tTest\tResult\tCode\tBaseline\n";
    for (const Result &item : expected)
    {
        const unsigned current = nes.cpuRead(item.address);
        const unsigned status = current & 3;
        const unsigned previous = item.baseline & 3;
        if (status != 1 && status != 2)
        {
            std::cerr << "Unexpected result status.\n";
            return 2;
        }
        passed += status == 1;
        failed += status == 2;
        improved += status == 1 && previous == 2;
        regressed += status == 2 && previous == 1;
        std::cout << item.page << '\t' << item.name << '\t' << (status == 1 ? "PASS" : "FAIL")
                  << '\t' << std::hex << std::uppercase << (current >> 2) << std::dec
                  << '\t' << (previous == 1 ? "PASS" : "FAIL") << '\n';
    }
    if (passed != nes.cpuRead(0x38))
    {
        std::cerr << "Result count does not match ROM tally.\n";
        return 2;
    }
    std::cout << "PASS " << passed << "/147; FAIL " << failed << "; improved " << improved
              << "; regressed " << regressed << '\n';
    return regressed ? 1 : 0;
}