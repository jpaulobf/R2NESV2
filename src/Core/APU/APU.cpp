#include "Core/APU/APU.h"
#include "Core/Bus/Bus.h"

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
    }

    APU::APU()
    {
        setAudioSampleRate(audioSampleRate);
        reset();
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
        dmc.timer = DmcPeriodsNtsc[0] - 1;
        dmc.timerReload = DmcPeriodsNtsc[0] - 1;
        noise.shiftRegister = 1;
        noise.timerReload = (NoisePeriodsNtsc[0] / 2) - 1;

        frameClockCounter = 0;
        frameCounterMode = 4;
        apuCyclePhase = false;
        frameIrqEnabled = false;
        frameIrqFlag = false;
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
            const uint16_t cpuPeriod = NoisePeriodsNtsc[data & 0x0F];
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
            dmc.timerReload = DmcPeriodsNtsc[dmc.rateIndex] - 1;
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
            dmc.enabled = (data & 0x10) != 0;
            if (dmc.enabled)
            {
                if (dmc.bytesRemaining == 0)
                {
                    dmc.currentAddress = dmc.sampleAddress;
                    dmc.bytesRemaining = dmc.sampleLength;
                }
                requestDmcDma();
            }
            else
            {
                dmc.bytesRemaining = 0;
                dmc.dmaPending = false;
            }
            break;

        case 0x4017:
            pendingFrameCounterMode = (data & 0x80) ? 5 : 4;
            frameIrqEnabled = (data & 0x40) == 0;
            if (!frameIrqEnabled)
                frameIrqFlag = false;

            // O reset do sequenciador ocorre depois de 3 ou 4 ciclos da CPU,
            // conforme a fase par/ímpar em que a escrita aconteceu. O step()
            // deste mesmo ciclo também consome um dos ciclos de atraso.
            frameCounterWriteDelay = static_cast<uint8_t>((apuCyclePhase ? 3 : 4) + 1);
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
            frameIrqFlag = false;
        return status;
    }

    void APU::step()
    {
        if (frameCounterWritePending)
        {
            if (frameCounterWriteDelay > 0)
                --frameCounterWriteDelay;
            if (frameCounterWriteDelay == 0)
                applyFrameCounterWrite();
        }

        bool quarterFrame = false;
        bool halfFrame = false;
        ++frameClockCounter;

        if (frameCounterMode == 4)
        {
            if (frameClockCounter == 7457)
                quarterFrame = true;
            else if (frameClockCounter == 14913)
                quarterFrame = halfFrame = true;
            else if (frameClockCounter == 22371)
                quarterFrame = true;
            else if (frameClockCounter == 29829)
            {
                quarterFrame = halfFrame = true;
                if (frameIrqEnabled)
                    frameIrqFlag = true;
                frameClockCounter = 0;
            }
        }
        else
        {
            if (frameClockCounter == 7457)
                quarterFrame = true;
            else if (frameClockCounter == 14913)
                quarterFrame = halfFrame = true;
            else if (frameClockCounter == 22371)
                quarterFrame = true;
            else if (frameClockCounter == 37281)
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
        dmc.clock();
        requestDmcDma();

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
            audioBuffer.push(filtered);
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

    void APU::setAudioSampleRate(float rate)
    {
        if (!std::isfinite(rate) || rate < 8000.0f || rate > 192000.0f)
            return;

        audioSampleRate = rate;
        apuCyclesPerSample = NtscCpuClockRate / static_cast<double>(rate);
        hpf90.init(rate, 90.0f, true);

        const double antiAliasCutoff = std::min(14000.0, static_cast<double>(rate) * 0.45);
        constexpr double ButterworthQ[] = {0.517638090205, 0.707106781187, 1.93185165258};
        for (size_t i = 0; i < antiAliasFilters.size(); ++i)
            antiAliasFilters[i].init(NtscCpuClockRate, antiAliasCutoff, ButterworthQ[i]);

        resetAudioPipeline();
    }

    void APU::completeDmcDma(uint8_t value, uint8_t totalStallCycles)
    {
        if (!dmc.dmaPending || !dmc.enabled || dmc.bytesRemaining == 0)
            return;

        dmc.sampleBuffer = value;
        dmc.sampleBufferEmpty = false;
        dmc.dmaPending = false;

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
        }

        // O ciclo da leitura do byte já foi consumido pelo NES; os ciclos
        // restantes são contabilizados nos próximos passos da CPU.
        dmc.dmaStallCyclesRemaining = totalStallCycles > 0
                                          ? static_cast<uint8_t>(totalStallCycles - 1)
                                          : 0;
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
        const float p1 = userPulse1Enabled ? static_cast<float>(pulse1.sample(true)) : 0.0f;
        const float p2 = userPulse2Enabled ? static_cast<float>(pulse2.sample(false)) : 0.0f;
        const float tri = userTriangleEnabled ? static_cast<float>(triangle.sample()) : 0.0f;
        const float n = userNoiseEnabled ? static_cast<float>(noise.sample()) : 0.0f;
        const float d = userDMCEnabled ? static_cast<float>(dmc.sample()) : 0.0f;

        const float pulseSum = p1 + p2;
        const float pulseOut = pulseSum > 0.0f
                                   ? 95.88f / (8128.0f / pulseSum + 100.0f)
                                   : 0.0f;

        const float tndDenominator = tri / 8227.0f + n / 12241.0f + d / 22638.0f;
        const float tndOut = tndDenominator > 0.0f
                                 ? 159.79f / (1.0f / tndDenominator + 100.0f)
                                 : 0.0f;
        return pulseOut + tndOut;
    }

    void APU::saveState(std::ostream &os)
    {
        auto write = [&os](const auto &value) { writeValue(os, value); };
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
        write(frameCounterWriteDelay);
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
    }

    void APU::loadState(std::istream &is)
    {
        auto read = [&is](auto &value) { readValue(is, value); };
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
        read(frameCounterWriteDelay);
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

        // O áudio já calculado pertence ao ponto temporal anterior ao load.
        audioBuffer = std::queue<float>();
    }
}
