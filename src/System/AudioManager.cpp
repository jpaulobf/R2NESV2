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

    // SDL converte as amostras mono float da APU para a saída padrão do sistema.
    void AudioManager::initialize()
    {
        close();
        const SDL_AudioSpec format{SDL_AUDIO_F32, 1, sampleRate};
        audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
        if (audioStream)
        {
            // SDL abre o dispositivo pausado. A reprodução só começa com reserva.
            std::cout << "Audio: Device stream opened successfully" << std::endl;
        }
        else
        {
            std::cerr << "Audio: Failed to open device! SDL_Error: " << SDL_GetError() << std::endl;
        }
    }

    // Fecha o dispositivo somente quando sua criação foi bem-sucedida.
    void AudioManager::close()
    {
        if (audioStream)
        {
            SDL_DestroyAudioStream(audioStream);
            audioStream = nullptr;
        }
        audioBuffer.clear();
        playbackStarted = false;
    }

    // Agrupa amostras para reduzir chamadas à fila SDL durante a geração do quadro.
    void AudioManager::pushSample(float sample)
    {
        audioBuffer.push_back(sample);
    }

    // Absorve pequenas variações de custo dos quadros sem cortar áudio pendente.
    void AudioManager::queueAudio()
    {
        if (audioStream && !audioBuffer.empty())
        {
            const int bytesPerSecond = sampleRate * static_cast<int>(sizeof(float));
            const int startupBytes = bytesPerSecond / 20; // ~3 quadros / 50 ms.
            const int maximumBytes = bytesPerSecond / 4;  // Recuperação de atraso >250 ms.
            const int queued = SDL_GetAudioStreamQueued(audioStream);
            if (queued < 0)
            {
                std::cerr << "Audio: Could not query queued samples: " << SDL_GetError() << std::endl;
                audioBuffer.clear();
                return;
            }

            // Um esgotamento real precisa reconstruir a reserva, assim como uma pausa.
            // O limite antigo de 50 ms descartava áudio válido após pequenos atrasos.
            if (queued > maximumBytes ||
                (playbackStarted && queued == 0 && SDL_GetAudioStreamAvailable(audioStream) == 0))
                resetStream();

            if (!SDL_PutAudioStreamData(audioStream, audioBuffer.data(), static_cast<int>(audioBuffer.size() * sizeof(float))))
                std::cerr << "Audio: Could not queue samples: " << SDL_GetError() << std::endl;
            else if (!playbackStarted && SDL_GetAudioStreamQueued(audioStream) >= startupBytes)
            {
                if (SDL_ResumeAudioStreamDevice(audioStream))
                    playbackStarted = true;
                else
                    std::cerr << "Audio: Could not start playback: " << SDL_GetError() << std::endl;
            }
        }
        audioBuffer.clear();
    }

    // Remove amostras pendentes, por exemplo após pausar ou trocar de ROM.
    void AudioManager::clearQueuedAudio()
    {
        audioBuffer.clear();
        resetStream();
    }

    void AudioManager::resetStream()
    {
        if (audioStream)
        {
            if (!SDL_PauseAudioStreamDevice(audioStream))
                std::cerr << "Audio: Could not pause playback: " << SDL_GetError() << std::endl;
            if (!SDL_ClearAudioStream(audioStream))
                std::cerr << "Audio: Could not clear queued samples: " << SDL_GetError() << std::endl;
        }
        playbackStarted = false;
    }
}