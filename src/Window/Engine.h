#pragma once

#include "Window.h"
#include "Core/NES.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
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
        Engine();

        ~Engine();

        void run();

        void toggleVSync();

        void toggleUncappedSpeed() { uncappedSpeed = !uncappedSpeed; }

    private:
        void processEmulatorInput();

        void handleKeyboard(SDL_Keycode key, bool isPressed);

        void init();

        void update();

        void render();

        void setFastForward(bool enabled);

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
        bool runningFastForward = false;
        bool soundEnabled = true;
        bool paused = false;
        bool oldUncappedSpeed = uncappedSpeed;
        bool oldVsyncEnabled = vsyncEnabled;

        // Controle dos sprites ilimitados
        bool unlimitedSprites = false;
    };
}