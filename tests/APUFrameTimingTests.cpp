#include "Core/NES.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
static void tick(NES &nes, int count = 1)
{
    for (int i = 0; i < count; ++i)
    {
        ++nes.getBus().systemClockCounter;
        nes.getApu().step();
    }
}
static uint8_t status(NES &nes, bool peek = true)
{
    return nes.getBus().cpuRead(0x4015, peek);
}
static void cadence(bool pal)
{
    NES nes("");
    nes.setPAL(pal);
    auto &apu = nes.getApu();
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4015, 1);
    apu.cpuWrite(0x4000, 0);
    apu.cpuWrite(0x4003, 0x28); // Length = 4.
    apu.cpuWrite(0x4017, 0x80);
    tick(nes, 4); // Clock manual: length = 3.
    apu.cpuWrite(0x4017, 0x40);
    tick(nes, 4); // Ciclo zero da sequência de quatro passos.
    const int thirdHalfClock = pal ? 49881 : 44743;
    tick(nes, thirdHalfClock - 1);
    check((status(nes) & 1) != 0, "length counter stopped before third half-frame clock");
    tick(nes);
    check((status(nes) & 1) == 0, "length counter missed third half-frame clock");
}
static void clearPhase(bool firstGet)
{
    NES nes("");
    auto &apu = nes.getApu();
    auto &bus = nes.getBus();
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4017, 0);
    tick(nes, 30020);
    check((status(nes) & 0x40) && apu.getIrqFlag(), "frame IRQ missing");
    if (((bus.systemClockCounter & 1) == 0) != firstGet)
        tick(nes);
    const uint32_t firstCycle = bus.systemClockCounter;
    check((status(nes) & 0x40) && (status(nes) & 0x40), "peek cleared frame IRQ");
    check((status(nes, false) & 0x40) != 0, "frame IRQ missing on read");
    tick(nes);
    if (firstGet)
    {
        check((status(nes) & 0x40) && apu.getIrqFlag(), "get read cleared IRQ before put");
        std::ostringstream saved;
        apu.saveState(saved);
        check((status(nes, false) & 0x40) != 0, "put read missed pending IRQ");
        tick(nes);
        check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "put-to-get did not clear IRQ");
        std::istringstream restore(saved.str());
        apu.loadState(restore);
        check(bool(restore), "pending IRQ clear state rejected");
        bus.systemClockCounter = firstCycle + 1;
        tick(nes);
        check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "pending IRQ clear not restored");
    }
    else
        check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "put read did not clear at get");
}
static void inhibitedWindow()
{
    NES nes("");
    auto &apu = nes.getApu();
    apu.setAudioOutputSuppressed(true);
    // Reset inhibits IRQ; the first two cycles of the last step still drive
    // the status bit, while the interrupt line remains inactive.
    tick(nes, 29827);
    check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "inhibited status set too early");
    tick(nes);
    check((status(nes) & 0x40) && !apu.getIrqFlag(), "first inhibited status cycle missing");
    tick(nes);
    check((status(nes) & 0x40) && !apu.getIrqFlag(), "second inhibited status cycle missing");
    tick(nes);
    check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "inhibited status did not clear");
}
static void reassertion()
{
    NES nes("");
    auto &apu = nes.getApu();
    apu.setAudioOutputSuppressed(true);
    apu.cpuWrite(0x4017, 0);
    tick(nes, 4); // Reset da sequência.
    tick(nes, 29827);
    check(!(status(nes) & 0x40), "IRQ asserted before its window");
    tick(nes);
    check((status(nes, false) & 0x40) && apu.getIrqFlag(), "first frame IRQ assertion missing");
    tick(nes);
    check((status(nes) & 0x40) && apu.getIrqFlag(), "second frame IRQ assertion missing");
    tick(nes);
    check((status(nes) & 0x40) && apu.getIrqFlag(), "third frame IRQ assertion missing");
    status(nes, false);
    tick(nes, 2);
    check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "frame IRQ did not clear after assertion window");

    tick(nes, 30000);
    check(status(nes) & 0x40, "next frame IRQ missing");
    status(nes, false);
    apu.cpuWrite(0x4017, 0x40);
    check(!(status(nes) & 0x40) && !apu.getIrqFlag(), "IRQ inhibit did not clear immediately");
}
int main()
{
    try
    {
        cadence(false);
        cadence(true);
        clearPhase(true);
        clearPhase(false);
        inhibitedWindow();
        reassertion();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "APU frame timing tests passed\n";
}