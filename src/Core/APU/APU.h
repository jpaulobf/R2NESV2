#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <queue>

namespace R2NES::Core
{
    class Bus;

    class APU
    {
    public:
        APU();

        // Avança o estado do APU em um ciclo da CPU.
        void step();
        void reset();
        void connectBus(Bus *bus);

        void cpuWrite(uint16_t addr, uint8_t data);
        uint8_t cpuRead(uint16_t addr, bool readOnly = false);

        float getOutputSample();
        void setAudioSampleRate(float rate);

        bool hasSamples() const { return !audioBuffer.empty(); }
        bool getIrqFlag() const { return frameIrqFlag || dmc.irqFlag; }

        // Interface usada pelo NES para transferir bytes do canal DMC e roubar
        // ciclos da CPU sem acoplar o APU ao escalonador principal.
        bool hasDmcDmaRequest() const { return dmc.dmaPending; }
        uint16_t getDmcDmaAddress() const { return dmc.currentAddress; }
        void completeDmcDma(uint8_t value, uint8_t totalStallCycles);
        bool consumeDmcDmaStallCycle();

        // Preferências de áudio da interface, independentes dos bits escritos
        // pelo jogo nos registradores de habilitação dos canais.
        void enableSound();
        void disableSound();
        void setPulse1Enabled(bool enabled) { userPulse1Enabled = enabled; }
        void setPulse2Enabled(bool enabled) { userPulse2Enabled = enabled; }
        void setTriangleEnabled(bool enabled) { userTriangleEnabled = enabled; }
        void setNoiseEnabled(bool enabled) { userNoiseEnabled = enabled; }
        void setDMCEnabled(bool enabled) { userDMCEnabled = enabled; }

        void saveState(std::ostream &os);
        void loadState(std::istream &is);

    private:
        static constexpr double NtscCpuClockRate = 1789773.0;

        Bus *bus = nullptr;

        struct LengthCounter
        {
            uint8_t count = 0;
            bool halt = false;
            void tick();
            void load(uint8_t code);
        };

        struct Envelope
        {
            bool start = false;
            bool loop = false;
            bool constantVolume = false;
            uint8_t volume = 0;
            uint8_t decayCount = 0;
            uint8_t dividerCount = 0;
            void tick();
            uint8_t getVolume() const;
        };

        struct Sweep
        {
            bool enabled = false;
            bool down = false;
            bool reload = false;
            uint8_t shift = 0;
            uint8_t timer = 0;
            uint8_t period = 0;
            void tick(uint16_t &pulseTimer, bool isPulse1);
            bool isSilencing(uint16_t pulseTimer, bool isPulse1) const;
        };

        struct PulseChannel
        {
            bool enabled = false;
            uint16_t timer = 0;
            uint16_t timerReload = 0;
            uint8_t dutyMode = 0;
            uint8_t dutyValue = 0;
            Envelope envelope;
            Sweep sweep;
            LengthCounter lengthCounter;
            void clock();
            uint8_t sample(bool isPulse1) const;
        };

        struct TriangleChannel
        {
            bool enabled = false;
            uint16_t timer = 0;
            uint16_t timerReload = 0;
            uint8_t dutyValue = 0;
            uint8_t linearCount = 0;
            uint8_t linearReload = 0;
            bool linearControl = false;
            bool linearReloadFlag = false;
            LengthCounter lengthCounter;
            void clock();
            uint8_t sample() const;
        };

        struct NoiseChannel
        {
            bool enabled = false;
            uint16_t timer = 0;
            uint16_t timerReload = 0;
            uint16_t shiftRegister = 1;
            bool mode = false;
            Envelope envelope;
            LengthCounter lengthCounter;
            void clock();
            uint8_t sample() const;
        };

        struct DMCChannel
        {
            bool enabled = false;
            bool irqEnabled = false;
            bool loop = false;
            bool irqFlag = false;
            bool sampleBufferEmpty = true;
            bool silence = true;
            bool dmaPending = false;
            uint8_t rateIndex = 0;
            uint8_t outputLevel = 0;
            uint8_t sampleBuffer = 0;
            uint8_t shiftRegister = 0;
            uint8_t bitsRemaining = 8;
            uint16_t timer = 0;
            uint16_t timerReload = 427;
            uint16_t sampleAddress = 0xC000;
            uint16_t sampleLength = 1;
            uint16_t currentAddress = 0xC000;
            uint16_t bytesRemaining = 0;
            uint8_t dmaStallCyclesRemaining = 0;

            uint8_t sample() const { return outputLevel; }
            void clock();
        };

        struct FirstOrderFilter
        {
            float alpha = 0.0f;
            float prevX = 0.0f;
            float prevY = 0.0f;
            bool isHighPass = true;

            void init(float sampleRate, float cutoffFreq, bool highPass);
            void reset();
            float process(float x);
        };

        // A biquad passabaixa executado na frequência do APU antes da redução
        // para a taxa de saída, evitando aliases que um filtro posterior não
        // conseguiria remover.
        struct BiquadFilter
        {
            double b0 = 1.0;
            double b1 = 0.0;
            double b2 = 0.0;
            double a1 = 0.0;
            double a2 = 0.0;
            double z1 = 0.0;
            double z2 = 0.0;

            void init(double sampleRate, double cutoffFreq, double q);
            void reset();
            float process(float x);
        };

        PulseChannel pulse1;
        PulseChannel pulse2;
        TriangleChannel triangle;
        NoiseChannel noise;
        DMCChannel dmc;

        FirstOrderFilter hpf90;
        std::array<BiquadFilter, 3> antiAliasFilters;
        std::queue<float> audioBuffer;

        float sampleSum = 0.0f;
        uint32_t sampleCount = 0;
        double apuCyclesPerSample = NtscCpuClockRate / 44100.0;
        double cycleCounter = 0.0;
        float audioSampleRate = 44100.0f;

        uint32_t frameClockCounter = 0;
        uint8_t frameCounterMode = 4;
        bool apuCyclePhase = false;
        bool frameIrqEnabled = false;
        bool frameIrqFlag = false;
        bool frameCounterWritePending = false;
        uint8_t frameCounterWriteDelay = 0;
        uint8_t pendingFrameCounterMode = 4;

        bool soundEnabled = true;
        bool userPulse1Enabled = true;
        bool userPulse2Enabled = true;
        bool userTriangleEnabled = true;
        bool userNoiseEnabled = true;
        bool userDMCEnabled = true;

        void clockQuarterFrame();
        void clockHalfFrame();
        void applyFrameCounterWrite();
        void requestDmcDma();
        void resetAudioPipeline();
        size_t maxBufferedSamples() const;
        float getRawMix();
    };
}
