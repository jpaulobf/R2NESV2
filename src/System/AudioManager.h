#pragma once

#include <SDL.h>
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

        // Enfileira o lote pendente ou o descarta durante fast-forward.
        void queueAudio(bool isFastForwarding);

        // Descarta o áudio ainda aguardando reprodução no dispositivo SDL.
        void clearQueuedAudio();

        // Retorna a taxa efetiva negociada com o dispositivo de áudio.
        int getSampleRate() const { return sampleRate; }

    private:
        // Identificador do dispositivo aberto; zero representa ausência de dispositivo.
        SDL_AudioDeviceID audioDevice = 0;

        // Acumula amostras até a Engine finalizar o quadro atual.
        std::vector<float> audioBuffer;

        // Valor padrão usado caso a negociação com SDL não substitua a frequência.
        int sampleRate = 44100;
    };
}
