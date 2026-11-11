#include "Core/APU/APU.h"
#include "Core/Bus/Bus.h"
#include "Core/Cartridge/Cartridge.h"
#include "Core/Memory/StateRead.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>

namespace R2NES::Core
{
    namespace
    {
        constexpr uint8_t LengthTable[] = {
            10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
            12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

        // Períodos expressos em ciclos de CPU. O timer de Noise é clockado a
        // cada segundo ciclo, portanto seus valores de reload são convertidos.
        constexpr uint16_t NoisePeriodsNtsc[] = {
            4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

        // Períodos do timer DMC em ciclos de CPU para as taxas NTSC.
        constexpr uint16_t DmcPeriodsNtsc[] = {
            428, 380, 340, 320, 286, 254, 226, 214,
            190, 160, 142, 128, 106, 84, 72, 54};

        constexpr uint16_t NoisePeriodsPal[] = {4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778};
        constexpr uint16_t DmcPeriodsPal[] = {398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50};
        constexpr uint8_t DutySequences[4][8] = {
            {0, 1, 0, 0, 0, 0, 0, 0},
            {0, 1, 1, 0, 0, 0, 0, 0},
            {0, 1, 1, 1, 1, 0, 0, 0},
            {1, 0, 0, 1, 1, 1, 1, 1}};

        template <typename Stream, typename Value>
        void writeValue(Stream &stream, const Value &value)
        {
            stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
        }

        template <typename Stream, typename Value>
        void readValue(Stream &stream, Value &value)
        {
            stream.read(reinterpret_cast<char *>(&value), sizeof(value));
        }

        void readValue(std::istream &stream, bool &value)
        {
            readStateBool(stream, value);
        }
    }

    APU::APU()
    {
        setAudioSampleRate(audioSampleRate);
        reset();
    }

    void APU::setPAL(bool enabled)
    {
        pal = enabled;
        setAudioSampleRate(audioSampleRate);
    }

    void APU::reset()
    {
        // Preserva preferências da interface, conexão com o barramento e taxa
        // de saída; todo o estado emulado dos canais volta ao padrão.
        pulse1 = PulseChannel{};
        pulse2 = PulseChannel{};
        triangle = TriangleChannel{};
        noise = NoiseChannel{};
        dmc = DMCChannel{};
        // A primeira recarga NTSC ocorre um ciclo depois da fase usada
        // anteriormente. A DMA continua alinhada ao put; a escrita em $4010
        // no ciclo da recarga deve fornecer o novo período.
        dmc.timer = pal ? DmcPeriodsPal[0] - 1 : DmcPeriodsNtsc[0];
        dmc.timerReload = (pal ? DmcPeriodsPal[0] : DmcPeriodsNtsc[0]) - 1;
        noise.shiftRegister = 1;
        noise.timerReload = ((pal ? NoisePeriodsPal[0] : NoisePeriodsNtsc[0]) / 2) - 1;

        frameClockCounter = 0;
        frameCounterMode = 4;
        apuCyclePhase = false;
        frameIrqEnabled = false;
        frameIrqFlag = false;
        frameIrqClearPending = false;
        frameCounterWritePending = false;
        frameCounterWriteDelay = 0;
        pendingFrameCounterMode = 4;

        resetAudioPipeline();
    }

    void APU::connectBus(Bus *b)
    {
        bus = b;
    }

    void APU::cpuWrite(uint16_t addr, uint8_t data)
    {
        switch (addr)
        {
        case 0x4000:
            pulse1.dutyMode = data >> 6;
            pulse1.lengthCounter.halt = (data & 0x20) != 0;
            pulse1.envelope.loop = (data & 0x20) != 0;
            pulse1.envelope.constantVolume = (data & 0x10) != 0;
            pulse1.envelope.volume = data & 0x0F;
            break;
        case 0x4001:
            pulse1.sweep.enabled = (data & 0x80) != 0;
            pulse1.sweep.period = (data >> 4) & 0x07;
            pulse1.sweep.down = (data & 0x08) != 0;
            pulse1.sweep.shift = data & 0x07;
            pulse1.sweep.reload = true;
            break;
        case 0x4002:
            pulse1.timerReload = (pulse1.timerReload & 0xFF00) | data;
            break;
        case 0x4003:
            pulse1.timerReload = (pulse1.timerReload & 0x00FF) | (static_cast<uint16_t>(data & 0x07) << 8);
            if (pulse1.enabled)
                pulse1.lengthCounter.load(data >> 3);
            pulse1.timer = pulse1.timerReload;
            pulse1.dutyValue = 0;
            pulse1.envelope.start = true;
            break;

        case 0x4004:
            pulse2.dutyMode = data >> 6;
            pulse2.lengthCounter.halt = (data & 0x20) != 0;
            pulse2.envelope.loop = (data & 0x20) != 0;
            pulse2.envelope.constantVolume = (data & 0x10) != 0;
            pulse2.envelope.volume = data & 0x0F;
            break;
        case 0x4005:
            pulse2.sweep.enabled = (data & 0x80) != 0;
            pulse2.sweep.period = (data >> 4) & 0x07;
            pulse2.sweep.down = (data & 0x08) != 0;
            pulse2.sweep.shift = data & 0x07;
            pulse2.sweep.reload = true;
            break;
        case 0x4006:
            pulse2.timerReload = (pulse2.timerReload & 0xFF00) | data;
            break;
        case 0x4007:
            pulse2.timerReload = (pulse2.timerReload & 0x00FF) | (static_cast<uint16_t>(data & 0x07) << 8);
            if (pulse2.enabled)
                pulse2.lengthCounter.load(data >> 3);
            pulse2.timer = pulse2.timerReload;
            pulse2.dutyValue = 0;
            pulse2.envelope.start = true;
            break;

        case 0x4008:
            triangle.linearControl = (data & 0x80) != 0;
            triangle.lengthCounter.halt = triangle.linearControl;
            triangle.linearReload = data & 0x7F;
            break;
        case 0x400A:
            triangle.timerReload = (triangle.timerReload & 0xFF00) | data;
            break;
        case 0x400B:
            triangle.timerReload = (triangle.timerReload & 0x00FF) | (static_cast<uint16_t>(data & 0x07) << 8);
            if (triangle.enabled)
                triangle.lengthCounter.load(data >> 3);
            triangle.timer = triangle.timerReload;
            triangle.linearReloadFlag = true;
            break;

        case 0x400C:
            noise.envelope.loop = (data & 0x20) != 0;
            noise.lengthCounter.halt = noise.envelope.loop;
            noise.envelope.constantVolume = (data & 0x10) != 0;
            noise.envelope.volume = data & 0x0F;
            break;
        case 0x400E:
        {
            noise.mode = (data & 0x80) != 0;
            const uint16_t cpuPeriod = (pal ? NoisePeriodsPal[data & 0x0F] : NoisePeriodsNtsc[data & 0x0F]);
            noise.timerReload = static_cast<uint16_t>((cpuPeriod / 2) - 1);
            break;
        }
        case 0x400F:
            if (noise.enabled)
                noise.lengthCounter.load(data >> 3);
            noise.envelope.start = true;
            break;

        case 0x4010:
            dmc.irqEnabled = (data & 0x80) != 0;
            dmc.loop = (data & 0x40) != 0;
            dmc.rateIndex = data & 0x0F;
            dmc.timerReload = (pal ? DmcPeriodsPal[dmc.rateIndex] : DmcPeriodsNtsc[dmc.rateIndex]) - 1;
            if (!dmc.irqEnabled)
                dmc.irqFlag = false;
            break;
        case 0x4011:
            dmc.outputLevel = data & 0x7F;
            break;
        case 0x4012:
            dmc.sampleAddress = static_cast<uint16_t>(0xC000 | (static_cast<uint16_t>(data) << 6));
            break;
        case 0x4013:
            dmc.sampleLength = static_cast<uint16_t>((static_cast<uint16_t>(data) << 4) | 1);
            break;

        case 0x4015:
            pulse1.enabled = (data & 0x01) != 0;
            if (!pulse1.enabled)
                pulse1.lengthCounter.count = 0;
            pulse2.enabled = (data & 0x02) != 0;
            if (!pulse2.enabled)
                pulse2.lengthCounter.count = 0;
            triangle.enabled = (data & 0x04) != 0;
            if (!triangle.enabled)
                triangle.lengthCounter.count = 0;
            noise.enabled = (data & 0x08) != 0;
            if (!noise.enabled)
                noise.lengthCounter.count = 0;

            // Toda escrita em $4015 reconhece a IRQ do DMC.
            dmc.irqFlag = false;
            if (!pal && !(data & 0x10) && dmc.bytesRemaining > 0 &&
                !dmc.sampleBufferEmpty && dmc.bitsRemaining == 1 &&
                !dmc.dmaPending)
            {
                // O decodificador DMA ainda vê a última amostra quando a
                // desativação coincide com a expiração do timer. Um ou dois
                // ciclos antes, ele produz somente um halt abortado.
                if (dmc.timer == 0)
                    dmc.dmaPending = true;
                else if (dmc.timer <= 2)
                    dmc.abortArmed = true;
            }
            dmc.enabled = (data & 0x10) != 0;
            if (dmc.enabled)
            {
                const bool startingLoad = dmc.bytesRemaining == 0 && dmc.sampleBufferEmpty && !dmc.dmaPending;
                if (dmc.bytesRemaining == 0)
                {
                    dmc.currentAddress = dmc.sampleAddress;
                    dmc.bytesRemaining = dmc.sampleLength;
                }
                requestDmcDma();
                // A primeira tentativa ocorre na segunda fase get da APU
                // após a escrita, três ou quatro ciclos de CPU depois dela.
                if (startingLoad && dmc.dmaPending && !pal && bus)
                    dmc.loadDelay = (bus->systemClockCounter & 1) ? 3 : 4;
            }
            else
            {
                dmc.bytesRemaining = 0;
                // Uma transferência já agendada atravessa a escrita de stop.
                // Uma carga ainda em espera, porém, não começa.
                if (dmc.loadDelay != 0)
                    dmc.dmaPending = false;
                dmc.loadDelay = 0;
            }
            break;

        case 0x4017:
            pendingFrameCounterMode = (data & 0x80) ? 5 : 4;
            frameIrqEnabled = (data & 0x40) == 0;
            if (!frameIrqEnabled)
            {
                frameIrqFlag = false;
                frameIrqClearPending = false;
            }

            // Inclui o step() do ciclo de escrita na contagem de 3 ou 4
            // ciclos, conforme a fase. A CPU escreve antes do step da APU.
            frameCounterWriteDelay = static_cast<uint8_t>(apuCyclePhase ? 3 : 4);
            frameCounterWritePending = true;
            break;
        }
    }

    uint8_t APU::cpuRead(uint16_t addr, bool readOnly)
    {
        if (addr != 0x4015)
            return 0x00;

        uint8_t status = 0;
        if (pulse1.lengthCounter.count > 0)
            status |= 0x01;
        if (pulse2.lengthCounter.count > 0)
            status |= 0x02;
        if (triangle.lengthCounter.count > 0)
            status |= 0x04;
        if (noise.lengthCounter.count > 0)
            status |= 0x08;
        if (dmc.bytesRemaining > 0)
            status |= 0x10;
        if (frameIrqFlag)
            status |= 0x40;
        if (dmc.irqFlag)
            status |= 0x80;

        // O bit 5 é open bus no hardware; o barramento atual não mantém uma
        // trava geral de open bus, então ele permanece zero nesta implementação.
        if (!readOnly)
        {
            if (pal)
                frameIrqFlag = false;
            else
                frameIrqClearPending = true;
        }
        return status;
    }

    void APU::step()
    {
        // Em NTSC, a leitura de $4015 agenda a limpeza para a transição
        // seguinte de put para get. A CPU acessa o barramento antes do step;
        // nesse ponto o contador do sistema já aponta para o próximo ciclo.
        if (frameIrqClearPending && (bus ? (bus->systemClockCounter & 1) == 0 : apuCyclePhase))
        {
            frameIrqFlag = false;
            frameIrqClearPending = false;
        }
        bool frameCounterReset = false;
        if (frameCounterWritePending)
        {
            if (frameCounterWriteDelay > 0)
                --frameCounterWriteDelay;
            if (frameCounterWriteDelay == 0)
            {
                applyFrameCounterWrite();
                frameCounterReset = true;
            }
        }

        bool quarterFrame = false;
        bool halfFrame = false;
        // O ciclo que aplica o reset é o ciclo zero da nova sequência.
        if (!frameCounterReset)
            ++frameClockCounter;

        if (frameCounterMode == 4)
        {
            // O sinal de IRQ é afirmado nos três ciclos em torno do último
            // passo; uma leitura no meio dessa janela pode ser seguida por
            // uma nova afirmação da flag.
            const uint32_t lastStep = pal ? 33253 : 29829;
            if (frameClockCounter >= lastStep - 1 && frameClockCounter <= lastStep + 1)
            {
                if (!pal && !frameIrqEnabled)
                    frameIrqFlag = frameClockCounter <= lastStep;
                else if (frameIrqEnabled)
                    frameIrqFlag = true;
            }
            if (frameClockCounter == (pal ? 8313 : 7457))
                quarterFrame = true;
            else if (frameClockCounter == (pal ? 16627 : 14913))
                quarterFrame = halfFrame = true;
            else if (frameClockCounter == (pal ? 24939 : 22371))
                quarterFrame = true;
            else if (frameClockCounter == (pal ? 33253 : 29829))
            {
                quarterFrame = halfFrame = true;
            }
            else if (frameClockCounter == (pal ? 33254 : 29830))
            {
                frameClockCounter = 0;
            }
        }
        else
        {
            if (frameClockCounter == (pal ? 8313 : 7457))
                quarterFrame = true;
            else if (frameClockCounter == (pal ? 16627 : 14913))
                quarterFrame = halfFrame = true;
            else if (frameClockCounter == (pal ? 24939 : 22371))
                quarterFrame = true;
            else if (frameClockCounter == (pal ? 41565 : 37281))
            {
                quarterFrame = halfFrame = true;
                frameClockCounter = 0;
            }
        }

        if (quarterFrame)
            clockQuarterFrame();
        if (halfFrame)
            clockHalfFrame();

        // Pulse, Noise e DMC usam a fase do clock do APU, independente do
        // contador do frame. Triangle avança em todo ciclo da CPU.
        if (apuCyclePhase)
        {
            pulse1.clock();
            pulse2.clock();
            noise.clock();
        }
        apuCyclePhase = !apuCyclePhase;

        triangle.clock();
        const bool dmcOutputEnds = dmc.timer == 0 && dmc.bitsRemaining == 1;
        dmc.clock();
        if (dmcOutputEnds && dmc.abortArmed)
        {
            dmc.abortArmed = false;
            dmc.abortPending = true;
            dmc.dmaPending = true;
        }
        if (dmc.loadDelay > 0)
            --dmc.loadDelay;
        // Recargas param a CPU em uma fase put. O contador já aponta
        // para o próximo ciclo de sistema quando a APU é avançada.
        if (pal || !bus || (bus->systemClockCounter & 1))
            requestDmcDma();

        // Os canais, o sequenciador e o DMC continuam acima. Fast-forward
        // dispensa mistura, filtros, resampling e armazenamento de amostras.
        if (audioOutputSuppressed)
            return;

        const double cyclesPerSample = apuCyclesPerSample;
        cycleCounter += 1.0;
        if (soundEnabled)
        {
            float mixed = getRawMix();
            for (auto &filter : antiAliasFilters)
                mixed = filter.process(mixed);
            sampleSum += mixed;
            ++sampleCount;
        }

        if (cycleCounter >= cyclesPerSample)
        {
            cycleCounter -= cyclesPerSample;
            if (!soundEnabled)
            {
                sampleSum = 0.0f;
                sampleCount = 0;
                return;
            }

            float averageMix = 0.0f;
            if (sampleCount > 0)
                averageMix = sampleSum / static_cast<float>(sampleCount);
            sampleSum = 0.0f;
            sampleCount = 0;

            const float filtered = hpf90.process(averageMix) * 1.2f;
            if (audioBuffer.size() >= maxBufferedSamples())
                audioBuffer.pop();
            audioBuffer.push(filtered * userVolumes[0]);
        }
    }

    float APU::getOutputSample()
    {
        if (audioBuffer.empty())
            return 0.0f;

        // Soft clipping evita o corte abrupto que gerava harmônicos adicionais.
        const float sample = audioBuffer.front();
        audioBuffer.pop();
        return std::tanh(sample);
    }

    void APU::clearPendingSamples()
    {
        audioBuffer = std::queue<float>();
    }

    void APU::setUserVolume(size_t index, int percent)
    {
        if (index < userVolumes.size())
            userVolumes[index] = std::clamp(percent, 0, 100) / 100.0f;
    }

    void APU::enableSound()
    {
        if (!soundEnabled)
            resetAudioPipeline();
        soundEnabled = true;
    }

    void APU::disableSound()
    {
        if (soundEnabled)
            resetAudioPipeline();
        soundEnabled = false;
    }

    void APU::setAudioOutputSuppressed(bool suppressed)
    {
        if (audioOutputSuppressed == suppressed)
            return;

        audioOutputSuppressed = suppressed;
        resetAudioPipeline();
    }

    void APU::setAudioSampleRate(float rate)
    {
        if (!std::isfinite(rate) || rate < 8000.0f || rate > 192000.0f)
            return;

        audioSampleRate = rate;
        apuCyclesPerSample = cpuClockRate() / static_cast<double>(rate);
        hpf90.init(rate, 90.0f, true);

        const double antiAliasCutoff = std::min(14000.0, static_cast<double>(rate) * 0.45);
        constexpr double ButterworthQ[] = {0.517638090205, 0.707106781187, 1.93185165258};
        for (size_t i = 0; i < antiAliasFilters.size(); ++i)
            antiAliasFilters[i].init(cpuClockRate(), antiAliasCutoff, ButterworthQ[i]);

        resetAudioPipeline();
    }

    void APU::completeDmcDma(uint8_t value, uint8_t totalStallCycles)
    {
        if (!dmc.dmaPending)
            return;
        if (!dmc.enabled || dmc.bytesRemaining == 0)
        {
            // Na leitura extra do RP2A03G tardio, o endereço não avança,
            // mas a segunda cópia do byte entra no buffer de reprodução.
            if (dmc.enabled && dmc.bytesRemaining == 0)
            {
                dmc.sampleBuffer = value;
                dmc.sampleBufferEmpty = false;
            }
            dmc.dmaPending = false;
            dmc.loadDelay = 0;
            return;
        }

        const bool lastByteNearOutputEnd = !pal && !dmc.loop &&
                                           dmc.bytesRemaining == 1 && dmc.bitsRemaining == 1;
        const uint16_t timerAtGet = dmc.timer;
        const uint16_t fetchedAddress = dmc.currentAddress;
        dmc.sampleBuffer = value;
        dmc.sampleBufferEmpty = false;
        dmc.dmaPending = false;
        dmc.loadDelay = 0;

        dmc.currentAddress = (dmc.currentAddress == 0xFFFF)
                                 ? 0x8000
                                 : static_cast<uint16_t>(dmc.currentAddress + 1);
        --dmc.bytesRemaining;

        if (dmc.bytesRemaining == 0)
        {
            if (dmc.loop)
            {
                dmc.currentAddress = dmc.sampleAddress;
                dmc.bytesRemaining = dmc.sampleLength;
            }
            else if (dmc.irqEnabled)
            {
                dmc.irqFlag = true;
            }
            if (lastByteNearOutputEnd)
            {
                if (timerAtGet == 0)
                {
                    dmc.currentAddress = fetchedAddress;
                    dmc.dmaPending = true; // RP2A03G tardio: leitura extra.
                }
                else if (timerAtGet <= 2)
                    dmc.abortArmed = true;
            }
        }

        // O ciclo da leitura do byte já foi consumido pelo NES; os ciclos
        // restantes são contabilizados nos próximos passos da CPU.
        dmc.dmaStallCyclesRemaining = totalStallCycles > 0
                                          ? static_cast<uint8_t>(totalStallCycles - 1)
                                          : 0;
    }

    void APU::finishDmcDmaAbort()
    {
        dmc.dmaPending = false;
        dmc.abortPending = false;
    }

    bool APU::consumeDmcDmaStallCycle()
    {
        if (dmc.dmaStallCyclesRemaining == 0)
            return false;
        --dmc.dmaStallCyclesRemaining;
        return true;
    }

    void APU::LengthCounter::tick()
    {
        if (!halt && count > 0)
            --count;
    }

    void APU::LengthCounter::load(uint8_t code)
    {
        count = LengthTable[code & 0x1F];
    }

    void APU::Envelope::tick()
    {
        if (start)
        {
            start = false;
            decayCount = 15;
            dividerCount = volume;
            return;
        }

        if (dividerCount == 0)
        {
            dividerCount = volume;
            if (decayCount == 0)
            {
                if (loop)
                    decayCount = 15;
            }
            else
            {
                --decayCount;
            }
        }
        else
        {
            --dividerCount;
        }
    }

    uint8_t APU::Envelope::getVolume() const
    {
        return constantVolume ? volume : decayCount;
    }

    bool APU::Sweep::isSilencing(uint16_t pulseTimer, bool) const
    {
        if (pulseTimer < 8)
            return true;

        const uint16_t delta = pulseTimer >> shift;
        return !down && pulseTimer + delta > 0x7FF;
    }

    void APU::Sweep::tick(uint16_t &pulseTimer, bool isPulse1)
    {
        const uint16_t delta = pulseTimer >> shift;
        uint16_t targetTimer = pulseTimer;
        if (down)
        {
            targetTimer = static_cast<uint16_t>(targetTimer - delta);
            if (isPulse1)
                targetTimer = static_cast<uint16_t>(targetTimer - 1);
        }
        else
        {
            targetTimer = static_cast<uint16_t>(targetTimer + delta);
        }

        if (timer == 0 && enabled && shift > 0 && pulseTimer >= 8 && targetTimer <= 0x7FF)
            pulseTimer = targetTimer;

        if (timer == 0 || reload)
        {
            timer = period;
            reload = false;
        }
        else
        {
            --timer;
        }
    }

    void APU::PulseChannel::clock()
    {
        if (timerReload < 8)
            return;

        if (timer == 0)
        {
            timer = timerReload;
            dutyValue = (dutyValue + 1) & 0x07;
        }
        else
        {
            --timer;
        }
    }

    uint8_t APU::PulseChannel::sample(bool isPulse1) const
    {
        if (!enabled || lengthCounter.count == 0 || sweep.isSilencing(timerReload, isPulse1))
            return 0;
        return DutySequences[dutyMode][dutyValue] ? envelope.getVolume() : 0;
    }

    void APU::TriangleChannel::clock()
    {
        if (timer == 0)
        {
            timer = timerReload;
            if (lengthCounter.count > 0 && linearCount > 0)
                dutyValue = (dutyValue + 1) & 0x1F;
        }
        else
        {
            --timer;
        }
    }

    uint8_t APU::TriangleChannel::sample() const
    {
        return dutyValue < 16 ? static_cast<uint8_t>(15 - dutyValue)
                              : static_cast<uint8_t>(dutyValue - 16);
    }

    void APU::NoiseChannel::clock()
    {
        if (timer == 0)
        {
            timer = timerReload;
            const uint16_t tap = mode ? 6 : 1;
            const uint16_t feedback = (shiftRegister & 1) ^ ((shiftRegister >> tap) & 1);
            shiftRegister = static_cast<uint16_t>((shiftRegister >> 1) | (feedback << 14));
        }
        else
        {
            --timer;
        }
    }

    uint8_t APU::NoiseChannel::sample() const
    {
        if (!enabled || lengthCounter.count == 0 || (shiftRegister & 1))
            return 0;
        return envelope.getVolume();
    }

    void APU::DMCChannel::clock()
    {
        if (timer == 0)
        {
            timer = timerReload;

            if (!silence)
            {
                if (shiftRegister & 1)
                {
                    if (outputLevel <= 125)
                        outputLevel += 2;
                }
                else if (outputLevel >= 2)
                {
                    outputLevel -= 2;
                }
            }

            shiftRegister >>= 1;
            if (bitsRemaining > 0)
                --bitsRemaining;

            if (bitsRemaining == 0)
            {
                bitsRemaining = 8;
                if (sampleBufferEmpty)
                {
                    silence = true;
                }
                else
                {
                    silence = false;
                    shiftRegister = sampleBuffer;
                    sampleBufferEmpty = true;
                }
            }
        }
        else
        {
            --timer;
        }
    }

    void APU::FirstOrderFilter::init(float sampleRate, float cutoffFreq, bool highPass)
    {
        isHighPass = highPass;
        const float dt = 1.0f / sampleRate;
        const float rc = 1.0f / (2.0f * 3.14159265358979323846f * cutoffFreq);
        alpha = isHighPass ? rc / (rc + dt) : dt / (rc + dt);
        reset();
    }

    void APU::FirstOrderFilter::reset()
    {
        prevX = 0.0f;
        prevY = 0.0f;
    }

    float APU::FirstOrderFilter::process(float x)
    {
        const float y = isHighPass ? alpha * (prevY + x - prevX)
                                   : prevY + alpha * (x - prevY);
        prevX = x;
        prevY = y;
        return y;
    }

    void APU::BiquadFilter::init(double sampleRate, double cutoffFreq, double q)
    {
        const double omega = 2.0 * 3.14159265358979323846 * cutoffFreq / sampleRate;
        const double cosine = std::cos(omega);
        const double alpha = std::sin(omega) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        b0 = ((1.0 - cosine) * 0.5) / a0;
        b1 = (1.0 - cosine) / a0;
        b2 = b0;
        a1 = (-2.0 * cosine) / a0;
        a2 = (1.0 - alpha) / a0;
        reset();
    }

    void APU::BiquadFilter::reset()
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    float APU::BiquadFilter::process(float x)
    {
        const double output = b0 * x + z1;
        z1 = b1 * x - a1 * output + z2;
        z2 = b2 * x - a2 * output;
        return static_cast<float>(output);
    }

    void APU::clockQuarterFrame()
    {
        pulse1.envelope.tick();
        pulse2.envelope.tick();
        noise.envelope.tick();

        if (triangle.linearReloadFlag)
            triangle.linearCount = triangle.linearReload;
        else if (triangle.linearCount > 0)
            --triangle.linearCount;
        if (!triangle.linearControl)
            triangle.linearReloadFlag = false;
    }

    void APU::clockHalfFrame()
    {
        pulse1.lengthCounter.tick();
        pulse2.lengthCounter.tick();
        triangle.lengthCounter.tick();
        noise.lengthCounter.tick();
        pulse1.sweep.tick(pulse1.timerReload, true);
        pulse2.sweep.tick(pulse2.timerReload, false);
    }

    void APU::applyFrameCounterWrite()
    {
        frameCounterWritePending = false;
        frameCounterMode = pendingFrameCounterMode;
        frameClockCounter = 0;

        // No modo de cinco passos, os clocks iniciais ocorrem junto com o reset.
        if (frameCounterMode == 5)
        {
            clockQuarterFrame();
            clockHalfFrame();
        }
    }

    void APU::requestDmcDma()
    {
        if (dmc.enabled && dmc.sampleBufferEmpty && dmc.bytesRemaining > 0 && !dmc.dmaPending)
            dmc.dmaPending = true;
    }

    void APU::resetAudioPipeline()
    {
        sampleSum = 0.0f;
        sampleCount = 0;
        cycleCounter = 0.0;
        audioBuffer = std::queue<float>();
        hpf90.reset();
        for (auto &filter : antiAliasFilters)
            filter.reset();
    }

    size_t APU::maxBufferedSamples() const
    {
        return std::max<size_t>(512, static_cast<size_t>(audioSampleRate * 0.1f));
    }

    float APU::getRawMix()
    {
        const float p1 = userPulse1Enabled ? static_cast<float>(pulse1.sample(true)) * userVolumes[1] : 0.0f;
        const float p2 = userPulse2Enabled ? static_cast<float>(pulse2.sample(false)) * userVolumes[2] : 0.0f;
        const float tri = userTriangleEnabled ? static_cast<float>(triangle.sample()) * userVolumes[3] : 0.0f;
        const float n = userNoiseEnabled ? static_cast<float>(noise.sample()) * userVolumes[4] : 0.0f;
        const float d = userDMCEnabled ? static_cast<float>(dmc.sample()) * userVolumes[5] : 0.0f;

        const float pulseSum = p1 + p2;
        const float pulseOut = pulseSum > 0.0f
                                   ? 95.88f / (8128.0f / pulseSum + 100.0f)
                                   : 0.0f;

        const float tndDenominator = tri / 8227.0f + n / 12241.0f + d / 22638.0f;
        const float tndOut = tndDenominator > 0.0f
                                 ? 159.79f / (1.0f / tndDenominator + 100.0f)
                                 : 0.0f;
        // A sa�da do cartucho � somada antes dos filtros e do resampling.
        const float expansion = bus && bus->cart && bus->cart->getMapper()
                                    ? bus->cart->getMapper()->expansionAudio() * 0.30f
                                    : 0.0f;
        return pulseOut + tndOut + expansion;
    }

    void APU::saveState(std::ostream &os)
    {
        auto write = [&os](const auto &value)
        { writeValue(os, value); };
        auto saveLength = [&](const LengthCounter &length)
        {
            write(length.count);
            write(length.halt);
        };
        auto saveEnvelope = [&](const Envelope &envelope)
        {
            write(envelope.start);
            write(envelope.loop);
            write(envelope.constantVolume);
            write(envelope.volume);
            write(envelope.decayCount);
            write(envelope.dividerCount);
        };
        auto savePulse = [&](const PulseChannel &pulse)
        {
            write(pulse.enabled);
            write(pulse.timer);
            write(pulse.timerReload);
            write(pulse.dutyMode);
            write(pulse.dutyValue);
            saveLength(pulse.lengthCounter);
            saveEnvelope(pulse.envelope);
            write(pulse.sweep.enabled);
            write(pulse.sweep.down);
            write(pulse.sweep.reload);
            write(pulse.sweep.shift);
            write(pulse.sweep.timer);
            write(pulse.sweep.period);
        };

        write(frameClockCounter);
        write(frameCounterMode);
        write(apuCyclePhase);
        write(frameIrqEnabled);
        write(frameIrqFlag);
        write(frameCounterWritePending);
        // Bit 7 antes reservado permite salvar a limpeza pendente sem mudar
        // o tamanho do estado nem invalidar estados anteriores comuns.
        const uint8_t savedFrameWriteDelay = frameCounterWriteDelay | (frameIrqClearPending ? 0x80 : 0);
        write(savedFrameWriteDelay);
        write(pendingFrameCounterMode);
        write(cycleCounter);
        write(sampleSum);
        write(sampleCount);

        savePulse(pulse1);
        savePulse(pulse2);

        write(triangle.enabled);
        write(triangle.timer);
        write(triangle.timerReload);
        write(triangle.dutyValue);
        write(triangle.linearCount);
        write(triangle.linearReload);
        write(triangle.linearControl);
        write(triangle.linearReloadFlag);
        saveLength(triangle.lengthCounter);

        write(noise.enabled);
        write(noise.timer);
        write(noise.timerReload);
        write(noise.shiftRegister);
        write(noise.mode);
        saveEnvelope(noise.envelope);
        saveLength(noise.lengthCounter);

        write(dmc.enabled);
        write(dmc.irqEnabled);
        write(dmc.loop);
        write(dmc.irqFlag);
        write(dmc.sampleBufferEmpty);
        write(dmc.silence);
        write(dmc.dmaPending);
        write(dmc.rateIndex);
        write(dmc.outputLevel);
        write(dmc.sampleBuffer);
        write(dmc.shiftRegister);
        write(dmc.bitsRemaining);
        write(dmc.timer);
        write(dmc.timerReload);
        write(dmc.sampleAddress);
        write(dmc.sampleLength);
        write(dmc.currentAddress);
        write(dmc.bytesRemaining);
        write(dmc.dmaStallCyclesRemaining);

        write(hpf90.prevX);
        write(hpf90.prevY);
        for (const auto &filter : antiAliasFilters)
        {
            write(filter.z1);
            write(filter.z2);
        }
        const uint8_t savedDmcLoadDelay = static_cast<uint8_t>(dmc.loadDelay |
                                                               (dmc.abortArmed ? 0x80 : 0) |
                                                               (dmc.abortPending ? 0x40 : 0));
        write(savedDmcLoadDelay);
    }

    void APU::loadState(std::istream &is)
    {
        auto read = [&is](auto &value)
        { readValue(is, value); };
        auto loadLength = [&](LengthCounter &length)
        {
            read(length.count);
            read(length.halt);
        };
        auto loadEnvelope = [&](Envelope &envelope)
        {
            read(envelope.start);
            read(envelope.loop);
            read(envelope.constantVolume);
            read(envelope.volume);
            read(envelope.decayCount);
            read(envelope.dividerCount);
        };
        auto loadPulse = [&](PulseChannel &pulse)
        {
            read(pulse.enabled);
            read(pulse.timer);
            read(pulse.timerReload);
            read(pulse.dutyMode);
            read(pulse.dutyValue);
            loadLength(pulse.lengthCounter);
            loadEnvelope(pulse.envelope);
            read(pulse.sweep.enabled);
            read(pulse.sweep.down);
            read(pulse.sweep.reload);
            read(pulse.sweep.shift);
            read(pulse.sweep.timer);
            read(pulse.sweep.period);
        };

        read(frameClockCounter);
        read(frameCounterMode);
        read(apuCyclePhase);
        read(frameIrqEnabled);
        read(frameIrqFlag);
        read(frameCounterWritePending);
        uint8_t savedFrameWriteDelay = 0;
        read(savedFrameWriteDelay);
        frameCounterWriteDelay = savedFrameWriteDelay & 0x7F;
        frameIrqClearPending = (savedFrameWriteDelay & 0x80) != 0;
        read(pendingFrameCounterMode);
        read(cycleCounter);
        read(sampleSum);
        read(sampleCount);

        loadPulse(pulse1);
        loadPulse(pulse2);

        read(triangle.enabled);
        read(triangle.timer);
        read(triangle.timerReload);
        read(triangle.dutyValue);
        read(triangle.linearCount);
        read(triangle.linearReload);
        read(triangle.linearControl);
        read(triangle.linearReloadFlag);
        loadLength(triangle.lengthCounter);

        read(noise.enabled);
        read(noise.timer);
        read(noise.timerReload);
        read(noise.shiftRegister);
        read(noise.mode);
        loadEnvelope(noise.envelope);
        loadLength(noise.lengthCounter);

        read(dmc.enabled);
        read(dmc.irqEnabled);
        read(dmc.loop);
        read(dmc.irqFlag);
        read(dmc.sampleBufferEmpty);
        read(dmc.silence);
        read(dmc.dmaPending);
        read(dmc.rateIndex);
        read(dmc.outputLevel);
        read(dmc.sampleBuffer);
        read(dmc.shiftRegister);
        read(dmc.bitsRemaining);
        read(dmc.timer);
        read(dmc.timerReload);
        read(dmc.sampleAddress);
        read(dmc.sampleLength);
        read(dmc.currentAddress);
        read(dmc.bytesRemaining);
        read(dmc.dmaStallCyclesRemaining);

        read(hpf90.prevX);
        read(hpf90.prevY);
        for (auto &filter : antiAliasFilters)
        {
            read(filter.z1);
            read(filter.z2);
        }
        uint8_t savedDmcLoadDelay = 0;
        read(savedDmcLoadDelay);
        dmc.loadDelay = savedDmcLoadDelay & 0x0F;
        dmc.abortArmed = (savedDmcLoadDelay & 0x80) != 0;
        dmc.abortPending = (savedDmcLoadDelay & 0x40) != 0;

        const auto validEnvelope = [](const Envelope &envelope)
        {
            return envelope.volume <= 15 && envelope.decayCount <= 15 && envelope.dividerCount <= 15;
        };
        const auto validSweep = [](const Sweep &sweep)
        {
            return sweep.shift <= 7 && sweep.period <= 7 && sweep.timer <= 7;
        };
        bool finiteFilters = std::isfinite(hpf90.prevX) && std::isfinite(hpf90.prevY);
        for (const auto &filter : antiAliasFilters)
            finiteFilters = finiteFilters && std::isfinite(filter.z1) && std::isfinite(filter.z2);

        // Validate counters and fields used by table lookups, shifts and filters.
        if (is && (pulse1.dutyMode > 3 || pulse2.dutyMode > 3 ||
                   pulse1.dutyValue > 7 || pulse2.dutyValue > 7 ||
                   !validSweep(pulse1.sweep) || !validSweep(pulse2.sweep) ||
                   !validEnvelope(pulse1.envelope) || !validEnvelope(pulse2.envelope) || !validEnvelope(noise.envelope) ||
                   triangle.dutyValue > 31 || dmc.rateIndex > 15 || dmc.outputLevel > 127 ||
                   triangle.linearCount > 127 || triangle.linearReload > 127 ||
                   dmc.bitsRemaining > 8 || dmc.dmaStallCyclesRemaining > 4 || dmc.loadDelay > 4 ||
                   (savedDmcLoadDelay & 0x30) != 0 ||
                   (dmc.abortArmed && dmc.abortPending) ||
                   (dmc.abortPending && (!dmc.dmaPending || dmc.loadDelay != 0)) ||
                   (pal && (dmc.abortArmed || dmc.abortPending)) ||
                   (dmc.loadDelay != 0 && (!dmc.dmaPending || pal)) ||
                   (frameCounterMode != 4 && frameCounterMode != 5) ||
                   (pendingFrameCounterMode != 4 && pendingFrameCounterMode != 5) ||
                   frameClockCounter >= (frameCounterMode == 4 ? (pal ? 33254u : 29830u) : (pal ? 41565u : 37281u)) ||
                   frameCounterWriteDelay > 5 || !std::isfinite(cycleCounter) || cycleCounter < 0.0 ||
                   cycleCounter >= cpuClockRate() / 8000.0 ||
                   sampleCount > static_cast<uint32_t>(std::ceil(cpuClockRate() / 8000.0)) ||
                   !std::isfinite(sampleSum) || !finiteFilters))
            is.setstate(std::ios::failbit);

        // O áudio já calculado pertence ao ponto temporal anterior ao load.
        audioBuffer = std::queue<float>();
    }
}