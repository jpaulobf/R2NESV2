#include "Core/APU/APU.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void configure(APU &apu)
{
    apu.cpuWrite(0x4017, 0); // Frame IRQ habilitada.
    apu.cpuWrite(0x4010, 0x8F); // DMC IRQ habilitada, taxa mais rápida.
    apu.cpuWrite(0x4012, 0x20);
    apu.cpuWrite(0x4013, 1); // 17 bytes de DMC.
    apu.cpuWrite(0x4015, 0x1F);
    apu.cpuWrite(0x4000, 0xBF);
    apu.cpuWrite(0x4002, 40);
    apu.cpuWrite(0x4003, 8);
    apu.cpuWrite(0x4004, 0x7A);
    apu.cpuWrite(0x4006, 80);
    apu.cpuWrite(0x4007, 8);
    apu.cpuWrite(0x4008, 0xFF);
    apu.cpuWrite(0x400A, 60);
    apu.cpuWrite(0x400B, 8);
    apu.cpuWrite(0x400C, 0x3F);
    apu.cpuWrite(0x400E, 4);
    apu.cpuWrite(0x400F, 8);
}

int main()
{
    try
    {
        APU reference, suppressed;
        configure(reference);
        configure(suppressed);
        suppressed.setAudioOutputSuppressed(true);
        int dmaRequests = 0;
        bool frameIrq = false, dmcIrq = false;
        for (int cycle = 0; cycle < 60000; ++cycle)
        {
            reference.step();
            suppressed.step();
            check(reference.hasDmcDmaRequest() == suppressed.hasDmcDmaRequest(), "DMC DMA timing changed");
            if (reference.hasDmcDmaRequest())
            {
                check(reference.getDmcDmaAddress() == suppressed.getDmcDmaAddress(), "DMC DMA address changed");
                reference.completeDmcDma(0xA5, 4);
                suppressed.completeDmcDma(0xA5, 4);
                ++dmaRequests;
            }
            check(reference.consumeDmcDmaStallCycle() == suppressed.consumeDmcDmaStallCycle(), "DMC stalls changed");
            const uint8_t status = reference.cpuRead(0x4015, true);
            check(status == suppressed.cpuRead(0x4015, true), "Channel status/IRQs changed");
            check(reference.getIrqFlag() == suppressed.getIrqFlag(), "IRQ flag changed");
            frameIrq |= (status & 0x40) != 0;
            dmcIrq |= (status & 0x80) != 0;
            check(!suppressed.hasSamples(), "Samples produced during suppression");
        }
        check(dmaRequests == 17 && frameIrq && dmcIrq, "DMA and IRQ cases were not exercised");
        check(reference.hasSamples(), "Reference did not produce audio");

        // Transições da Engine descartam apenas a fila, preservando inclusive
        // os filtros que fazem parte do estado restaurado.
        std::ostringstream beforeClear, afterClear;
        reference.saveState(beforeClear);
        reference.clearPendingSamples();
        reference.saveState(afterClear);
        check(!reference.hasSamples(), "Pending samples not cleared for transition");
        check(beforeClear.str() == afterClear.str(), "Queue clearing changed APU state or filters");
        for (int cycle = 0; cycle < 200; ++cycle)
            reference.step();
        check(reference.hasSamples(), "Queue clearing disabled audio output");
        // Avança o outro APU igualmente para a comparação de estado abaixo.
        for (int cycle = 0; cycle < 200; ++cycle)
            suppressed.step();

        // Normaliza apenas a saída de ambos e compara todos os bytes do estado:
        // timers, envelopes, sequências, contadores e DMC devem permanecer iguais.
        reference.setAudioOutputSuppressed(true);
        reference.setAudioOutputSuppressed(false);
        suppressed.setAudioOutputSuppressed(false);
        std::ostringstream referenceState, suppressedState;
        reference.saveState(referenceState);
        suppressed.saveState(suppressedState);
        check(referenceState.str() == suppressedState.str(), "Suppression changed emulated APU state");
        for (int cycle = 0; cycle < 200; ++cycle)
            suppressed.step();
        check(suppressed.hasSamples(), "Audio did not resume");
        suppressed.setAudioOutputSuppressed(true);
        check(!suppressed.hasSamples(), "Old samples not cleared");
        suppressed.disableSound();
        suppressed.setAudioOutputSuppressed(false);
        for (int cycle = 0; cycle < 200; ++cycle)
            suppressed.step();
        check(!suppressed.hasSamples(), "Leaving fast-forward enabled muted sound");
        suppressed.enableSound();
        for (int cycle = 0; cycle < 200; ++cycle)
            suppressed.step();
        check(suppressed.hasSamples(), "Sound preference could not be re-enabled");
        std::cout << "APU output suppression tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
