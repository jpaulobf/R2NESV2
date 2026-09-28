#include "AudioManager.h"
#include <iostream>

namespace R2NES::System
{
    // Inicializa a saída de áudio junto com o gerenciador.
    AudioManager::AudioManager()
    {
        initialize();
    }

    // Garante que o dispositivo SDL não sobreviva ao gerenciador.
    AudioManager::~AudioManager()
    {
        close();
    }

    // Solicita um dispositivo mono com amostras float e armazena a frequência concedida.
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

    // Fecha o dispositivo somente quando sua criação foi bem-sucedida.
    void AudioManager::close()
    {
        if (audioDevice > 0)
        {
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
        }
    }

    // Agrupa amostras para reduzir chamadas à fila SDL durante a geração do quadro.
    void AudioManager::pushSample(float sample)
    {
        audioBuffer.push_back(sample);
    }

    // Mantém a fila curta em velocidade normal e a silencia durante fast-forward.
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

    // Remove amostras pendentes, por exemplo após pausar ou trocar de ROM.
    void AudioManager::clearQueuedAudio()
    {
        if (audioDevice > 0)
        {
            SDL_ClearQueuedAudio(audioDevice);
        }
    }
}
