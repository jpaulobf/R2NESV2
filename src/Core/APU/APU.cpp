#include "Core/APU/APU.h"
#include "Core/Bus/Bus.h"
#include <cmath>

namespace R2NES::Core
{
    // Tabela de Length Counter (padrão do hardware NES)
    static const uint8_t lengthTable[] = {
        10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

    // Tabela de períodos para o canal de Noise (NTSC)
    static const uint16_t noiseTable[] = {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

    // Sequências de Duty Cycle para os canais Pulse
    static const uint8_t dutySequences[4][8] = {
        {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
        {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
        {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
        {1, 0, 0, 1, 1, 1, 1, 1}  // 25% invertido
    };

    APU::APU()
    {
        reset();
    }

    APU::~APU() {}

    void APU::reset()
    {
        frameClockCounter = 0;
        frameCounterMode = 4;
        irqEnabled = false;
        irqFlag = false;
        noise.shiftRegister = 1;

        // Inicializa timers com valores seguros para evitar clocks ultrassônicos no início
        pulse1.timerReload = 0;
        pulse2.timerReload = 0;
        triangle.timerReload = 0;
        noise.timerReload = 0;
        noise.timer = 1;

        // Reinicia estado do Slew Limiter
        lastPulse1Sample = 0.0f;
        lastPulse2Sample = 0.0f;
        lastTriangleSample = 0.0f;
        lastNoiseSample = 0.0f;
        lastDmcSample = 0.0f;
        highPassOutput = 0.0f;
        lowPassOutput = 0.0f;

        sampleSum = 0.0f;
        sampleCount = 0;

        audioBuffer = std::queue<float>(); // Limpa os restos de áudio
        cycleCounter = 0.0;
    }

    void APU::connectBus(Bus *b) { bus = b; }

    void APU::cpuWrite(uint16_t addr, uint8_t data)
    {
        switch (addr)
        {
        // Pulse 1
        case 0x4000:
            pulse1.dutyMode = (data >> 6);
            pulse1.lengthCounter.halt = (data & 0x20);
            pulse1.envelope.loop = (data & 0x20);
            pulse1.envelope.constantVolume = (data & 0x10);
            pulse1.envelope.volume = (data & 0x0F);
            break;
        case 0x4001:
            pulse1.sweep.enabled = (data & 0x80);
            pulse1.sweep.period = (data >> 4) & 0x07;
            pulse1.sweep.down = (data & 0x08);
            pulse1.sweep.shift = (data & 0x07);
            pulse1.sweep.reload = true;
            break;
        case 0x4002:
            pulse1.timerReload = (pulse1.timerReload & 0xFF00) | data;
            break;
        case 0x4003:
            pulse1.timerReload = (pulse1.timerReload & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (pulse1.enabled)
                pulse1.lengthCounter.load(data >> 3);
            // pulse1.dutyValue = 0;
            pulse1.envelope.start = true;
            break;

        // Pulse 2
        case 0x4004:
            pulse2.dutyMode = (data >> 6);
            pulse2.lengthCounter.halt = (data & 0x20);
            pulse2.envelope.loop = (data & 0x20);
            pulse2.envelope.constantVolume = (data & 0x10);
            pulse2.envelope.volume = (data & 0x0F);
            break;
        case 0x4005:
            pulse2.sweep.enabled = (data & 0x80);
            pulse2.sweep.period = (data >> 4) & 0x07;
            pulse2.sweep.down = (data & 0x08);
            pulse2.sweep.shift = (data & 0x07);
            pulse2.sweep.reload = true;
            break;
        case 0x4006:
            pulse2.timerReload = (pulse2.timerReload & 0xFF00) | data;
            break;
        case 0x4007:
            pulse2.timerReload = (pulse2.timerReload & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (pulse2.enabled)
                pulse2.lengthCounter.load(data >> 3);
            // pulse2.dutyValue = 0;
            pulse2.envelope.start = true;
            break;

        // Triangle
        case 0x4008:
            triangle.linearControl = (data & 0x80);
            triangle.lengthCounter.halt = (data & 0x80);
            triangle.linearReload = (data & 0x7F);
            break;
        case 0x400A:
            triangle.timerReload = (triangle.timerReload & 0xFF00) | data;
            break;
        case 0x400B:
            triangle.timerReload = (triangle.timerReload & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
            if (triangle.enabled)
                triangle.lengthCounter.load(data >> 3);
            triangle.linearReloadFlag = true;
            break;

        case 0x4011:
            dmc.sampleValue = (data & 0x7F);
            break;

        // Noise
        case 0x400C:
            noise.envelope.loop = (data & 0x20);
            noise.lengthCounter.halt = (data & 0x20);
            noise.envelope.constantVolume = (data & 0x10);
            noise.envelope.volume = (data & 0x0F);
            break;
        case 0x400E:
            noise.mode = (data & 0x80);
            noise.timerReload = noiseTable[data & 0x0F];
            break;
        case 0x400F:
            if (noise.enabled)
                noise.lengthCounter.load(data >> 3);
            noise.envelope.start = true;
            break;

        case 0x4015: // Status
            pulse1.enabled = (data & 0x01);
            if (!pulse1.enabled)
                pulse1.lengthCounter.count = 0;
            pulse2.enabled = (data & 0x02);
            if (!pulse2.enabled)
                pulse2.lengthCounter.count = 0;
            triangle.enabled = (data & 0x04);
            if (!triangle.enabled)
                triangle.lengthCounter.count = 0;
            noise.enabled = (data & 0x08);
            if (!noise.enabled)
                noise.lengthCounter.count = 0;
            break;

        case 0x4017: // Frame Counter
            frameCounterMode = (data & 0x80) ? 5 : 4;
            irqEnabled = !(data & 0x40);
            frameClockCounter = 0;
            if (frameCounterMode == 5)
            {
                pulse1.envelope.tick();
                pulse2.envelope.tick();
                noise.envelope.tick();
                pulse1.lengthCounter.tick();
                pulse2.lengthCounter.tick();
                triangle.lengthCounter.tick();
                noise.lengthCounter.tick();
                pulse1.sweep.tick(pulse1.timerReload, true);
                pulse2.sweep.tick(pulse2.timerReload, false);
            }
            break;
        }
    }

    uint8_t APU::cpuRead(uint16_t addr)
    {
        if (addr == 0x4015)
        {
            uint8_t res = 0;
            if (pulse1.lengthCounter.count > 0)
                res |= 0x01;
            if (pulse2.lengthCounter.count > 0)
                res |= 0x02;
            if (triangle.lengthCounter.count > 0)
                res |= 0x04;
            if (noise.lengthCounter.count > 0)
                res |= 0x08;
            // DMC Status...
            irqFlag = false; // Leitura limpa o flag de IRQ
            return res;
        }
        return 0x00;
    }

    void APU::step()
    {
        // O Frame Counter divide o tempo em steps (aprox. 240Hz ou 192Hz)
        // 1 cycle do APU = 1 cycle da CPU
        bool quarterFrame = false;
        bool halfFrame = false;

        frameClockCounter++;

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
                if (irqEnabled)
                    irqFlag = true;
                frameClockCounter = 0;
            }
        }
        else // Mode 5
        {
            if (frameClockCounter == 7457)
                quarterFrame = true;
            else if (frameClockCounter == 14913)
                quarterFrame = halfFrame = true;
            else if (frameClockCounter == 22371)
                quarterFrame = true;
            else if (frameClockCounter == 29829)
                ; // No-op step
            else if (frameClockCounter == 37281)
            {
                quarterFrame = halfFrame = true;
                frameClockCounter = 0;
            }
        }

        if (quarterFrame)
        {
            pulse1.envelope.tick();
            pulse2.envelope.tick();
            noise.envelope.tick();
            // Linear Counter do Triangle
            if (triangle.linearReloadFlag)
                triangle.linearCount = triangle.linearReload;
            else if (triangle.linearCount > 0)
                triangle.linearCount--;
            if (!triangle.linearControl)
                triangle.linearReloadFlag = false;
        }

        if (halfFrame)
        {
            pulse1.lengthCounter.tick();
            pulse2.lengthCounter.tick();
            triangle.lengthCounter.tick();
            noise.lengthCounter.tick();
            pulse1.sweep.tick(pulse1.timerReload, true);
            pulse2.sweep.tick(pulse2.timerReload, false);
        }

        // Clock dos canais (Timers)
        if (frameClockCounter % 2 == 0)
        {
            pulse1.clock();
            pulse2.clock();
            noise.clock();
        }
        triangle.clock(); // Triangle roda na frequência cheia

        if (soundEnabled)
        {
            sampleSum += getRawMix();
            sampleCount++;
        }
        cycleCounter += 1.0;

        // Quando o clock da CPU atingir o tempo exato de 1 amostra de áudio (ex: ~40.5 ciclos)
        if (cycleCounter >= apuCyclesPerSample)
        {
            cycleCounter -= apuCyclesPerSample; // Subtrai para manter a precisão fracionária

            if (!soundEnabled)
            {
                sampleSum = 0.0f;
                sampleCount = 0;
                return;
            }

            float averageMix = sampleSum / static_cast<float>(sampleCount);
            sampleSum = 0.0f;
            sampleCount = 0;

            // Filtros (Passa-Alta para centralizar o eixo, Passa-Baixa para limpar chiados)
            float filtered = hpf90.process(averageMix);
            filtered = lpf14000.process(filtered);

            // Um ganho mais conservador para evitar o som distorcido/metálico
            filtered *= 1.2f;

            // Envia para a fila em vez de esperar a placa de som pedir
            audioBuffer.push(filtered);
        }
    }

    float APU::getOutputSample()
    {
        if (audioBuffer.empty())
            return 0.0f; // Silêncio se o emulador estiver pausado ou lento

        float sample = audioBuffer.front();
        audioBuffer.pop();

        // Clipper suave de segurança para a placa de som
        if (sample > 1.0f)
            sample = 1.0f;
        if (sample < -1.0f)
            sample = -1.0f;

        return sample;
    }

    void APU::setAudioSampleRate(float rate)
    {
        if (rate > 0.0f)
        {
            audioSampleRate = rate;
            apuCyclesPerSample = 1789773.0 / rate;

            // Configura apenas o HPF para DC Offset e o LPF para os ruídos agudos
            hpf90.init(rate, 90.0f, true);
            lpf14000.init(rate, 14000.0f, false);
        }
    }

    float APU::getRawMix()
    {
        float p1 = userPulse1Enabled ? static_cast<float>(pulse1.sample()) : 0.0f;
        float p2 = userPulse2Enabled ? static_cast<float>(pulse2.sample()) : 0.0f;
        float tri = userTriangleEnabled ? static_cast<float>(triangle.sample()) : 0.0f;
        float n = userNoiseEnabled ? static_cast<float>(noise.sample()) : 0.0f;
        float d = userDMCEnabled ? static_cast<float>(dmc.sample()) : 0.0f;

        float pulseOut = 0.0f;
        float pulseSum = p1 + p2;
        if (pulseSum > 0.0f)
            pulseOut = 95.88f / (8128.0f / pulseSum + 100.0f);

        float tndOut = 0.0f;
        float tndDenominator = (tri / 8227.0f + n / 12241.0f + d / 22638.0f);
        if (tndDenominator > 0.0f)
            tndOut = 159.79f / (1.0f / tndDenominator + 100.0f);

        return pulseOut + tndOut;
    }

    void APU::setSlewMs(float ms)
    {
        if (ms >= 0.0f)
            slewMs = ms;
    }

    // --- Implementações auxiliares ---

    void APU::LengthCounter::tick()
    {
        if (!halt && count > 0)
            count--;
    }
    void APU::LengthCounter::load(uint8_t code)
    {
        count = lengthTable[code & 0x1F];
    }

    void APU::Envelope::tick()
    {
        if (!start)
        {
            if (dividerCount == 0)
            {
                dividerCount = volume;
                if (decayCount == 0)
                {
                    if (loop)
                        decayCount = 15;
                }
                else
                    decayCount--;
            }
            else
                dividerCount--;
        }
        else
        {
            start = false;
            decayCount = 15;
            dividerCount = volume;
        }
    }
    uint8_t APU::Envelope::getVolume() const
    {
        return constantVolume ? volume : decayCount;
    }

    bool APU::Sweep::isSilencing(uint16_t pulseTimer, bool isPulse1) const
    {
        if (pulseTimer < 8)
            return true;

        uint16_t delta = pulseTimer >> shift;
        if (!down)
        {
            if (pulseTimer + delta > 0x7FF)
                return true;
        }
        // O canal 1 e 2 têm comportamentos de muting ligeiramente diferentes no 'down',
        // mas a regra do 0x7FF no 'up' é a principal causadora de silêncio em notas agudas.
        return false;
    }

    void APU::Sweep::tick(uint16_t &pulseTimer, bool isPulse1)
    {
        uint16_t delta = pulseTimer >> shift;
        uint16_t targetTimer = pulseTimer;

        if (down)
        {
            targetTimer -= delta;
            if (isPulse1)
                targetTimer--;
        }
        else
        {
            targetTimer += delta;
        }

        if (timer == 0 && enabled && shift > 0 && pulseTimer >= 8 && targetTimer <= 0x7FF)
        {
            pulseTimer = targetTimer;
        }

        if (timer == 0 || reload)
        {
            timer = period;
            reload = false;
        }
        else
        {
            timer--;
        }
    }

    void APU::PulseChannel::clock()
    {
        // Se timerReload é muito pequeno, silencia (norma do NES)
        if (timerReload < 8)
            return;

        if (timer == 0)
        {
            timer = timerReload;
            dutyValue = (dutyValue + 1) % 8;
        }
        else
            timer--;
    }

    uint8_t APU::PulseChannel::sample() const
    {
        // Muting por: Enabled flag, Length Counter ou Unidade de Sweep
        if (!enabled || lengthCounter.count == 0 || sweep.isSilencing(timerReload, true))
            return 0;
        if (dutySequences[dutyMode][dutyValue] == 0)
            return 0;
        return envelope.getVolume();
    }

    void APU::TriangleChannel::clock()
    {
        if (timer == 0)
        {
            timer = timerReload;
            if (lengthCounter.count > 0 && linearCount > 0)
                dutyValue = (dutyValue + 1) % 32;
        }
        else
            timer--;
    }

    uint8_t APU::TriangleChannel::sample() const
    {
        if (dutyValue < 16)
            return 15 - dutyValue;
        return dutyValue - 16;
    }

    void APU::NoiseChannel::clock()
    {
        if (timer == 0)
        {
            timer = timerReload;
            uint8_t feedback = (shiftRegister & 0x01) ^ ((mode ? (shiftRegister >> 6) : (shiftRegister >> 1)) & 0x01);
            shiftRegister = (shiftRegister >> 1) | (feedback << 14);
        }
        else
            timer--;
    }

    uint8_t APU::NoiseChannel::sample() const
    {
        if (!enabled || lengthCounter.count == 0 || (shiftRegister & 0x01))
            return 0;
        return envelope.getVolume();
    }
}