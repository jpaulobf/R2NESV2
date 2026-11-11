#include "Core/NES.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void tickApuTo(NES &nes, uint32_t cycle)
{
    auto &bus = nes.getBus();
    while (bus.systemClockCounter < cycle)
    {
        ++bus.systemClockCounter;
        nes.getApu().step();
    }
}

static void setup(NES &nes, bool loop)
{
    auto &cpu = nes.getCpu();
    auto &bus = nes.getBus();
    auto &apu = nes.getApu();
    while (!cpu.complete())
        cpu.clock();
    bus.ram->write(0, 0x4C); // JMP $0000, only read cycles.
    bus.ram->write(1, 0);
    bus.ram->write(2, 0);
    cpu.pc = 0;
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4010, loop ? 0x4F : 0x0F);
    apu.cpuWrite(0x4015, 0x10);
    apu.completeDmcDma(0, 0);
    check(!apu.hasDmcDmaRequest(), "initial DMC fetch was not completed");
}

static std::string cpuState(NES &nes)
{
    std::ostringstream output;
    nes.getCpu().saveState(output);
    return output.str();
}

static void restoreApu(NES &nes)
{
    std::ostringstream output;
    nes.getApu().saveState(output);
    nes.getApu().reset();
    std::istringstream input(output.str());
    nes.getApu().loadState(input);
    check(bool(input), "DMC abort state was rejected");
}

static void explicitFullFetch()
{
    NES nes("");
    setup(nes, true);
    tickApuTo(nes, 806); // Timer is 0; stop on the output cycle.
    nes.getApu().cpuWrite(0x4015, 0);
    tickApuTo(nes, 807);
    check(nes.getApu().hasDmcDmaRequest() && !nes.getApu().isDmcDmaAbortPending(),
          "stop on output cycle did not preserve the full DMA");
    const auto before = cpuState(nes);
    for (int cycle = 0; cycle < 4; ++cycle)
    {
        nes.step(false);
        check(cpuState(nes) == before, "full DMA resumed CPU early");
    }
    check(!nes.getApu().hasDmcDmaRequest(), "full DMA did not finish");
    nes.step(false);
    check(cpuState(nes) != before, "CPU did not resume after full DMA");
}

static void explicitAbort(bool landOnWrite)
{
    NES nes("");
    setup(nes, true);
    tickApuTo(nes, 804); // Two timer counts before the output cycle.
    nes.getApu().cpuWrite(0x4015, 0);
    restoreApu(nes); // Preserve the armed abort across an APU snapshot.
    tickApuTo(nes, 807);
    check(nes.getApu().isDmcDmaAbortPending(), "one-cycle abort was not scheduled");
    restoreApu(nes); // Also preserve the pending halt.
    check(nes.getApu().isDmcDmaAbortPending(), "pending abort was not restored");
    if (landOnWrite)
    {
        auto &cpu = nes.getCpu();
        auto &bus = nes.getBus();
        bus.ram->write(0x100, 0x8D); // STA $0010.
        bus.ram->write(0x101, 0x10);
        bus.ram->write(0x102, 0x00);
        cpu.pc = 0x100;
        cpu.a = 0x5A;
        for (int cycle = 0; cycle < 3; ++cycle)
            cpu.clock();
        check(cpu.nextCycleIsWrite(), "CPU did not reach the write cycle");
        nes.step(false);
        check(bus.ram->read(0x10) == 0x5A, "abort blocked the CPU write");
        check(!nes.getApu().hasDmcDmaRequest(), "write did not cancel the abort");
    }
    else
    {
        const auto before = cpuState(nes);
        nes.step(false);
        check(cpuState(nes) == before, "abort did not halt CPU for one cycle");
        check(!nes.getApu().hasDmcDmaRequest(), "abort remained pending");
        nes.step(false);
        check(cpuState(nes) != before, "CPU did not resume after abort");
    }
}

static void implicitCases()
{
    for (const bool duplicate : {false, true})
    {
        NES nes("");
        setup(nes, false);
        tickApuTo(nes, duplicate ? 1238 : 1236);
        nes.getApu().cpuWrite(0x4015, 0x10);
        nes.getApu().completeDmcDma(0, 0); // Last byte of a one-byte sample.
        tickApuTo(nes, 1239);
        check(nes.getApu().hasDmcDmaRequest(), "implicit stop missed its second DMA");
        if (duplicate)
            check(nes.getApu().getDmcDmaAddress() == 0xC000,
                  "implicit extra DMA did not reread the same sample address");
        check(nes.getApu().isDmcDmaAbortPending() != duplicate,
              "implicit stop selected the wrong DMA duration");
        const auto before = cpuState(nes);
        const int stalledCycles = duplicate ? 4 : 1;
        for (int cycle = 0; cycle < stalledCycles; ++cycle)
        {
            nes.step(false);
            check(cpuState(nes) == before, "implicit stop resumed CPU early");
        }
        check(!nes.getApu().hasDmcDmaRequest(), "implicit stop DMA did not finish");
        nes.step(false);
        check(cpuState(nes) != before, "CPU did not resume after implicit stop");
    }
}

int main()
{
    try
    {
        explicitFullFetch();
        explicitAbort(false);
        explicitAbort(true);
        implicitCases();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "DMC abort tests passed\n";
}