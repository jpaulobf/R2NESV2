#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <queue>
#include <iostream>

namespace R2NES::Core
{
    class Bus;

    class APU
    {
    public:
        // Construtor / destrutor
        APU();
        ~APU();

        // Avança o estado do APU em 1 ciclo de CPU (chamado a cada passo do NES)
        void step();

        // Reseta o estado interno do APU (buffers, contadores, registradores)
        void reset();

        // Conecta o barramento do sistema para leituras/escritas
        void connectBus(Bus *bus);

        // Escrita/Leitura pelo CPU aos registradores do APU (0x4000 - 0x4017)
        void cpuWrite(uint16_t addr, uint8_t data);
        uint8_t cpuRead(uint16_t addr);

        // Retorna uma amostra pronta para a saída de áudio (pop da fila)
        float getOutputSample();

        // Configura taxa de amostragem da saída de áudio (ex.: 44100 Hz)
        void setAudioSampleRate(float rate);

        // Ajusta o Slew Limiter (em ms) para evitar cliques em transições abruptas
        void setSlewMs(float ms);

        // Consultas de estado
        bool hasSamples() const { return !audioBuffer.empty(); }
        bool getIrqFlag() const { return irqFlag; }

        // Controle global de som e canais (usado pela UI/usuario)
        void enableSound() { soundEnabled = true; }
        void disableSound() { soundEnabled = false; }
        void setPulse1Enabled(bool enabled) { userPulse1Enabled = enabled; }
        void setPulse2Enabled(bool enabled) { userPulse2Enabled = enabled; }
        void setTriangleEnabled(bool enabled) { userTriangleEnabled = enabled; }
        void setNoiseEnabled(bool enabled) { userNoiseEnabled = enabled; }
        void setDMCEnabled(bool enabled) { userDMCEnabled = enabled; }

        // Persistência do estado do APU (savestate)
        void saveState(std::ostream &os);
        void loadState(std::istream &is);

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
            uint8_t sample(bool isPulse1) const;
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

        struct FirstOrderFilter
        {
            float alpha = 0.0f;
            float prevX = 0.0f;
            float prevY = 0.0f;
            bool isHighPass = true;

            void init(float sampleRate, float cutoffFreq, bool highPass)
            {
                isHighPass = highPass;
                float dt = 1.0f / sampleRate;
                float rc = 1.0f / (2.0f * 3.14159f * cutoffFreq);
                if (isHighPass)
                {
                    alpha = rc / (rc + dt);
                }
                else
                {
                    alpha = dt / (rc + dt);
                }
                prevX = 0.0f;
                prevY = 0.0f;
            }

            float process(float x)
            {
                float y = 0.0f;
                if (isHighPass)
                {
                    y = alpha * (prevY + x - prevX);
                }
                else
                {
                    y = prevY + alpha * (x - prevY);
                }
                prevX = x;
                prevY = y;
                return y;
            }
        };

        FirstOrderFilter hpf90;
        FirstOrderFilter lpf14000;

        std::queue<float> audioBuffer;

        float sampleSum = 0.0f;
        int sampleCount = 0;
        double apuCyclesPerSample = 0.0;
        double cycleCounter = 0.0;

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
        bool soundEnabled = true;

        bool userPulse1Enabled = true;
        bool userPulse2Enabled = true;
        bool userTriangleEnabled = true;
        bool userNoiseEnabled = true;
        bool userDMCEnabled = true;

        float getRawMix();
    };
}