#include "Core/NES.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "TestFixtures.h"

using namespace R2NES::Core;
static void check(bool ok, const char *msg)
{
    if (!ok)
        throw std::runtime_error(msg);
}
static unsigned frame(NES &nes)
{
    nes.clearFrameComplete();
    unsigned count = 0;
    while (!nes.isFrameComplete() && count < 40000)
    {
        nes.step();
        ++count;
    }
    check(nes.isFrameComplete(), "frame timeout");
    return count;
}
int main(int argc, char **argv)
{
    try
    {
        check(argc > 1, "missing directory");
        std::filesystem::create_directories(argv[1]);
        std::vector<unsigned char> rom(16 + 32768 + 8192, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1a;
        rom[4] = 2;
        rom[5] = 1;
        rom[16 + 0x7ffc] = 0;
        rom[16 + 0x7ffd] = 0x80;
        const auto path = std::filesystem::path(argv[1]) / "timing.nes";
        {
            std::ofstream f(path, std::ios::binary);
            f.write((char *)rom.data(), rom.size());
        }
        FixtureNES nes;
        check(nes.insertCartridge(path.string()), "cartridge");
        frame(nes);
        auto ntsc = frame(nes);
        check(ntsc >= 29780 && ntsc <= 29782, "NTSC frame length");
        std::ostringstream old;
        check(nes.saveState(old), "NTSC state");
        nes.setPAL(true);
        frame(nes);
        auto f1 = frame(nes), f2 = frame(nes);
        check((f1 == 33247 && f2 == 33248) || (f1 == 33248 && f2 == 33247), "PAL frame length");
        for (int i = 0; i < 2; i++)
            nes.step();
        std::ostringstream saved;
        check(nes.saveState(saved), "PAL state");
        for (int i = 0; i < 100; i++)
            nes.step();
        std::ostringstream expected;
        check(nes.saveState(expected), "PAL state after steps");
        std::istringstream restore(saved.str());
        check(nes.loadState(restore), "PAL restore");
        for (int i = 0; i < 100; i++)
            nes.step();
        std::ostringstream actual;
        check(nes.saveState(actual) && actual.str() == expected.str(), "PAL phase restore");
        std::ostringstream mapperState, cartState;
        nes.getBus().cart->getMapper()->saveState(mapperState);
        nes.getBus().cart->saveState(cartState);
        auto legacyPal = saved.str();
        const size_t delayOffset = legacyPal.size() - mapperState.str().size() - cartState.str().size() - 10;
        legacyPal.erase(delayOffset, 1);
        legacyPal[0] = 'T'; // R2NT, antes de serializar o atraso inicial do DMC.
        std::istringstream previousPal(legacyPal);
        check(!nes.loadState(previousPal), "legacy PAL state accepted");
        std::ostringstream afterLegacyPal;
        check(nes.saveState(afterLegacyPal) && afterLegacyPal.str() == expected.str(), "legacy PAL state changed console");
        for (char version : {'N', 'P', 'R', 'T'})
        {
            auto legacy = saved.str();
            legacy[0] = version;
            std::istringstream input(legacy);
            check(!nes.loadState(input), "legacy PAL version accepted");
            std::ostringstream afterLegacy;
            check(nes.saveState(afterLegacy) && afterLegacy.str() == expected.str(), "legacy PAL version changed console");
        }
        std::istringstream wrong(old.str());
        check(!nes.loadState(wrong), "cross region state accepted");
        nes.setPAL(false);
        std::istringstream old2(old.str());
        check(nes.loadState(old2), "current NTSC state rejected");
        for (char version : {'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'})
        {
            auto legacy = old.str();
            legacy[0] = version;
            std::istringstream input(legacy);
            check(!nes.loadState(input), "legacy NTSC version accepted");
            std::ostringstream afterLegacy;
            check(nes.saveState(afterLegacy) && afterLegacy.str() == old.str(), "legacy NTSC state changed console");
        }
        std::cout << "Region timing tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}