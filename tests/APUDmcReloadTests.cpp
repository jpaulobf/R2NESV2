#include "Core/NES.h"
#include <iostream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void tick(NES &nes, uint32_t until)
{
    auto &bus = nes.getBus();
    while (bus.systemClockCounter < until)
    {
        ++bus.systemClockCounter;
        nes.getApu().step();
    }
}

static void checkRateWriteAtFirstReload(bool writeOnReload)
{
    NES nes("");
    auto &apu = nes.getApu();
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4010, 0x4F); // loop, 54 CPU cycles per output bit
    apu.cpuWrite(0x4015, 0x10);
    check(apu.hasDmcDmaRequest(), "initial DMC load was not requested");
    apu.completeDmcDma(0, 0); // one buffered byte, with the sample looping
    check(!apu.hasDmcDmaRequest(), "initial DMC load did not complete");

    // Writes occur before the APU step in NES::stepRegion. The first NTSC
    // timer reload is on cycle 429: a write on that cycle changes its period,
    // while a write on cycle 430 leaves that first reload at the old rate.
    tick(nes, writeOnReload ? 428 : 429);
    apu.cpuWrite(0x4010, 0x40); // loop, 428 CPU cycles per output bit

    const uint32_t firstReloadDma = writeOnReload ? 3425 : 3051;
    tick(nes, firstReloadDma - 1);
    check(!apu.hasDmcDmaRequest(), "DMC buffer emptied before the expected reload");
    tick(nes, firstReloadDma);
    check(apu.hasDmcDmaRequest(), "DMC buffer missed the expected reload cycle");
}

int main()
{
    try
    {
        checkRateWriteAtFirstReload(true);
        checkRateWriteAtFirstReload(false);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "APU DMC reload tests passed\n";
}