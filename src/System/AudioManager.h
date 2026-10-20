#pragma once

#include <SDL3/SDL.h>
#include <vector>

namespace R2NES::System
{
    // Gerencia o dispositivo SDL e a fila de amostras produzidas pela APU.
    class AudioManager
    {
    public:
        // Abre o dispositivo de áudio padrão.
        AudioManager();

        // Fecha o dispositivo de áudio caso ele ainda esteja ativo.
        ~AudioManager();

        // Configura o dispositivo para amostras mono em ponto flutuante.
        void initialize();

        // Libera o dispositivo SDL e invalida seu identificador.
        void close();

        // Acrescenta uma amostra da APU ao lote pendente do quadro atual.
        void pushSample(float sample);

        // Enfileira o lote pendente para reprodução.
        void queueAudio();

        // Descarta o áudio ainda aguardando reprodução no dispositivo SDL.
        void clearQueuedAudio();

        // Retorna a taxa das amostras da APU; SDL converte para o formato do dispositivo.
        int getSampleRate() const { return sampleRate; }

        // Retorna verdadeiro se o dispositivo de áudio foi inicializado com sucesso.
        bool isInitialized() const { return audioStream != nullptr; }

    private:
        friend struct AudioManagerTestAccess;

        // Interrompe o consumo e prepara uma nova reserva de áudio.
        void resetStream();

        // O stream criado por SDL_OpenAudioDeviceStream também possui o dispositivo.
        SDL_AudioStream *audioStream = nullptr;
        bool playbackStarted = false;

        // Acumula amostras até a Engine finalizar o quadro atual.
        std::vector<float> audioBuffer;

        // Formato de entrada do stream, independente da frequência física da saída.
        int sampleRate = 44100;
    };
}