#include "System/AudioManager.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

namespace R2NES::System
{
    // Permite controlar o consumo real do stream sem depender do relógio do driver.
    struct AudioManagerTestAccess
    {
        static SDL_AudioStream *stream(AudioManager &audio) { return audio.audioStream; }
    };
}

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

static void queue(R2NES::System::AudioManager &audio, int count)
{
    for (int i = 0; i < count; ++i)
        audio.pushSample((i % 32) / 64.0f);
    audio.queueAudio();
}

int main()
{
    try
    {
        check(SDL_Init(SDL_INIT_AUDIO), "Could not initialize dummy audio");
        {
            R2NES::System::AudioManager audio;
            check(audio.isInitialized(), "Audio stream did not open");
            auto *stream = R2NES::System::AudioManagerTestAccess::stream(audio);
            const auto device = SDL_GetAudioStreamDevice(stream);
            // Reproduz a política antiga: 100 ms pendentes eram descartados ao
            // chegar o próximo quadro, mesmo com o dispositivo sem consumir nada.
            queue(audio, 4410);
            check(SDL_PauseAudioStreamDevice(stream), "Could not freeze playback");
            check(SDL_ClearAudioStream(stream), "Could not reset fixture");
            queue(audio, 4410);
            queue(audio, 735);
            check(SDL_GetAudioStreamQueued(stream) == (4410 + 735) * int(sizeof(float)),
                  "Normal queue jitter discarded unplayed samples");

            audio.clearQueuedAudio();
            check(SDL_AudioDevicePaused(device), "Clearing audio left empty playback running");
            check(SDL_GetAudioStreamQueued(stream) == 0, "Clear retained stale samples");
            queue(audio, 735);
            check(SDL_AudioDevicePaused(device), "Playback resumed without a reserve after menu pause");
            queue(audio, 735);
            check(SDL_AudioDevicePaused(device), "Playback resumed before three frames were buffered");
            queue(audio, 735);
            check(!SDL_AudioDevicePaused(device), "Playback never resumed after priming");

            // Simula esgotamento da fila após um quadro caro (scanlines/rewind).
            check(SDL_PauseAudioStreamDevice(stream), "Could not freeze playback");
            check(SDL_ClearAudioStream(stream), "Could not simulate underrun");
            queue(audio, 735);
            check(SDL_AudioDevicePaused(device), "Underrun did not rebuild its reserve");
            queue(audio, 735);
            queue(audio, 735);
            check(!SDL_AudioDevicePaused(device), "Underrun recovery remained paused");

            // Repete as transições de menu para detectar estado residual.
            for (int i = 0; i < 12; ++i)
            {
                audio.clearQueuedAudio();
                queue(audio, 735);
                check(SDL_AudioDevicePaused(device), "Repeated pause lost priming state");
                queue(audio, 1470);
                check(!SDL_AudioDevicePaused(device), "Repeated resume stayed paused");
            }
        }
        SDL_Quit();
        std::cout << "Audio queue tests passed (continuity, priming, underrun, repeated pauses)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
