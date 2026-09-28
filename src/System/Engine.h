#pragma once

#include "Window/Window.h"
#include "Core/NES.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <deque>
#include "Core/IO/NESButtons.h"
#include <SDL.h>
#include "System/AudioManager.h"
#include "System/InputManager.h"
#include "System/GameStateManager.h"

namespace R2NES::Core
{
    class Engine
    {
    public:
        // Cria os subsistemas do emulador e aplica as configurações iniciais.
        Engine();

        // Libera a Engine e seus subsistemas gerenciados por smart pointers.
        ~Engine();

        // Executa o laço principal de entrada, emulação e renderização.
        void run();

        // Alterna o VSync por meio da janela, mantendo o estado sincronizado via callback.
        void toggleVSync();

        // Alterna a emulação sem limite de velocidade.
        void toggleUncappedSpeed() { uncappedSpeed = !uncappedSpeed; }

    private:
        // Processa eventos da janela, periféricos e solicitações de estado/ROM.
        void processEmulatorInput();

        // Encaminha teclas ao controle e trata os atalhos globais da Engine.
        void handleKeyboard(SDL_Keycode key, bool isPressed);

        // Direciona um evento de gatilho do controle SDL para o jogador correspondente.
        void handleJoystickTrigger(int playerNum, SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes);

        // Aplica gatilhos do primeiro controle físico.
        void handleJoystickTrigger1(SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes);

        // Aplica gatilhos do segundo controle físico.
        void handleJoystickTrigger2(SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes);

        // Registra callbacks da janela e sincroniza as opções iniciais com o NES.
        void init();

        // Emula um quadro, incluindo entrada, áudio e captura para rewind.
        void update();

        // Desenha o quadro e atualiza as janelas auxiliares de depuração.
        void render();

        // Ativa/desativa fast-forward, preservando as opções anteriores de velocidade e VSync.
        void setFastForward(bool enabled);

        // Ativa/desativa rewind e limpa o áudio pendente ao iniciá-lo.
        void setRewind(bool enabled);

        // Serializa periodicamente o estado atual e o mantém no histórico limitado de rewind.
        void captureRewindState();

        // Restaura o próximo estado do histórico quando o intervalo de rewind é atingido.
        bool updateRewind(double deltaTime);

        // Descarta o histórico de rewind e reinicializa seus contadores.
        void clearRewindStates();

        // Ponteiros
        std::unique_ptr<Window> window;
        std::unique_ptr<NES> nes;
        std::unique_ptr<R2NES::System::AudioManager> audioManager;
        std::unique_ptr<R2NES::System::InputManager> inputManager;
        std::unique_ptr<R2NES::System::GameStateManager> stateManager;

        // Controle
        bool stepByStep = false;
        bool stepRequested = false;
        bool isRunning = true;

        // Variáveis de controle de tempo e performance
        double residualTime = 0.0;
        double renderResidualTime = 0.0;
        float timeScale = 1.0f;   // 1.0 = Normal, 2.0 = Fast Forward, 0.5 = Slow Motion
        double targetUPS = 59.94; // Taxa real do NES NTSC
        double targetFPS = 60.0;  // Taxa de renderização desejada

        // Cálculo de FPS real
        float currentFPS = 0.0f;
        int frameCount = 0;
        float fpsTimer = 0.0f;

        // Flag para ignorar o limite de tempo (Fast Forward ilimitado)
        bool uncappedSpeed = false;
        bool vsyncEnabled = false;
        bool fastForwardEnabled = false;
        bool rewindEnabled = false;
        bool runningFastForward = false;
        bool runningRewind = false;
        bool soundEnabled = true;
        bool paused = false;
        bool oldUncappedSpeed = uncappedSpeed;
        bool oldVsyncEnabled = vsyncEnabled;

        // Histórico em memória: a frente é o estado mais antigo e o fim é o mais recente.
        std::deque<std::string> rewindStates;
        int framesSinceLastRewindState = 0;
        double rewindResidualTime = 0.0;
        int rewindStateIntervalFrames = 5;
        double rewindHistorySeconds = 10.0;
        double rewindIntervalSeconds = 0.05;

        // Controle dos sprites ilimitados
        bool unlimitedSprites = false;
        bool tilesEnabled = true;
        bool spritesEnabled = true;

        // Controle do Overclock
        bool cpuOverclockEnabled = false;
    };
}