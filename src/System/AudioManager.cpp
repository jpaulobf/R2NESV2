#include "AudioManager.h"
#include <iostream>

namespace R2NES::System
{
    AudioManager::AudioManager()
    {
        initialize();
    }

    AudioManager::~AudioManager()
    {
        close();
    }

    void AudioManager::initialize()
    {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = 44100;
        want.format = AUDIO_F32SYS;
        want.channels = 1;
        want.samples = 512;

        audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (audioDevice > 0)
        {
            std::cout << "Audio: Device opened successfully (ID: " << audioDevice << ")" << std::endl;
            SDL_PauseAudioDevice(audioDevice, 0);
            sampleRate = have.freq;
        }
        else
        {
            std::cerr << "Audio: Failed to open device! SDL_Error: " << SDL_GetError() << std::endl;
        }
    }

    void AudioManager::close()
    {
        if (audioDevice > 0)
        {
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
        }
    }

    void AudioManager::pushSample(float sample)
    {
        audioBuffer.push_back(sample);
    }

    void AudioManager::queueAudio(bool isFastForwarding)
    {
        if (audioDevice > 0 && !audioBuffer.empty())
        {
            if (!isFastForwarding)
            {
                // Latência alvo de ~3 frames (~50ms)
                Uint32 maxSafeBytes = sampleRate * sizeof(float) / 20;

                // Se o buffer engasgar e acumular áudio velho, limpamos
                if (SDL_GetQueuedAudioSize(audioDevice) > maxSafeBytes)
                {
                    SDL_ClearQueuedAudio(audioDevice);
                }

                SDL_QueueAudio(audioDevice, audioBuffer.data(), audioBuffer.size() * sizeof(float));
            }
            else
            {
                // Em Fast-Forward, ignoramos o áudio para não estourar os ouvidos e acelerar
                SDL_ClearQueuedAudio(audioDevice);
            }
        }
        audioBuffer.clear();
    }

    void AudioManager::clearQueuedAudio()
    {
        if (audioDevice > 0)
        {
            SDL_ClearQueuedAudio(audioDevice);
        }
    }
}
