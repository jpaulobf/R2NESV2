#include "Core/NES.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include "TestFixtures.h"

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void tick(FixtureNES &nes, uint32_t until)
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
    FixtureNES nes("");
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

static void restoreApu(FixtureNES &nes)
{
    std::ostringstream output;
    nes.getApu().saveState(output);
    nes.getApu().reset();
    std::istringstream input(output.str());
    nes.getApu().loadState(input);
    check(bool(input), "buffered DMC restart state was rejected");
}

static void checkBufferedRestart(uint32_t restartCycle)
{
    FixtureNES nes("");
    auto &apu = nes.getApu();
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4010, 0x4F);
    apu.cpuWrite(0x4015, 0x10);
    apu.completeDmcDma(0, 0);
    tick(nes, 790);
    apu.cpuWrite(0x4015, 0); // Stop well before the output unit empties the buffer.
    tick(nes, restartCycle);
    apu.cpuWrite(0x4015, 0x10);
    restoreApu(nes); // The activation delay exists before any DMA is pending.
    const uint32_t readyCycle = restartCycle + ((restartCycle & 1) ? 3 : 4);
    tick(nes, 807); // The buffered byte is consumed on this cycle.
    check(apu.hasDmcDmaRequest(), "buffered restart missed the reload request");
    check(!apu.isDmcDmaReady(), "buffered reload halted before activation");
    restoreApu(nes); // The delayed reload must also survive a snapshot.
    tick(nes, readyCycle - 1);
    check(!apu.isDmcDmaReady(), "buffered reload became ready too early");
    tick(nes, readyCycle);
    check(apu.isDmcDmaReady(), "buffered reload did not become ready on schedule");
}

int main()
{
    try
    {
        checkRateWriteAtFirstReload(true);
        checkRateWriteAtFirstReload(false);
        for (uint32_t cycle : {804u, 805u, 806u})
            checkBufferedRestart(cycle);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "APU DMC reload tests passed\n";
}