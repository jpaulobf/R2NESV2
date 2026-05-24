#pragma once
#include <cstdint>
#include <array>
#include <vector>

namespace R2NES::Core
{
    class Bus;

    class APU
    {
    public:
        APU();
        ~APU();

        void step();

        void reset();

        void connectBus(Bus *bus);

        void cpuWrite(uint16_t addr, uint8_t data);

        uint8_t cpuRead(uint16_t addr);

        float getOutputSample();

        void setAudioSampleRate(float rate);

        void setSlewMs(float ms);

    private:
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
            uint8_t sample() const;
        } pulse1, pulse2;

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
        } triangle;

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
        } noise;

        struct DMCChannel
        {
            bool enabled = false;
            uint8_t sampleValue = 0;
            uint8_t sample() const { return sampleValue; }
        } dmc;

        float audioSampleRate = 44100.0f;
        float slewMs = 0.7f;
        float lastPulse1Sample = 0.0f;
        float lastPulse2Sample = 0.0f;
        float lastTriangleSample = 0.0f;
        float lastNoiseSample = 0.0f;
        float lastDmcSample = 0.0f;
        
        // Filtros simples para o sinal final
        float highPassOutput = 0.0f;
        float lowPassOutput = 0.0f;

        uint32_t frameClockCounter = 0;
        uint8_t frameCounterMode = 0;
        bool irqEnabled = false;
        bool irqFlag = false;
    };
}