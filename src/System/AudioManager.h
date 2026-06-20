#pragma once

#include <SDL.h>
#include <vector>

namespace R2NES::System
{
    class AudioManager
    {
    public:
        AudioManager();
        ~AudioManager();

        void initialize();
        void close();

        void pushSample(float sample);
        void queueAudio(bool isFastForwarding);

        void clearQueuedAudio();

        int getSampleRate() const { return sampleRate; }

    private:
        SDL_AudioDeviceID audioDevice = 0;
        std::vector<float> audioBuffer;
        int sampleRate = 44100;
    };
}
