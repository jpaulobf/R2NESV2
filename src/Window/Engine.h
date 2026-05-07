#pragma once

#include "Window.h"
#include "Core/NESBoard.h"
#include <memory>
#include <map>
#include <string>
#include "Core/IO/NESButtons.h"

namespace R2NES::Core
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        // Inicia o loop principal
        void run();

    private:
        void processEmulatorInput();
        void handleKeyboard(SDL_Keycode key, bool isPressed);
        void handleJoystick(int playerNum, SDL_GameControllerButton button, bool isPressed);
        void handleJoystick1(SDL_GameControllerButton button, bool isPressed);
        void handleJoystick2(SDL_GameControllerButton button, bool isPressed);
        void update();
        void render();

        std::unique_ptr<Window> window;
        std::unique_ptr<NesBoard> nes;

        // Mapeamento de teclas para o Player 1 e 2
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player2KeyMap;

        // Mapeamento de botões de controle
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player1ControllerMap;
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player2ControllerMap;

        // Cache para armazenar o código traduzido da ROM
        std::map<uint16_t, std::string> cachedDisassembly;

        bool stepByStep = false;
        bool stepRequested = false;

        bool isRunning = true;

        // Variáveis de controle de tempo e performance
        double residualTime = 0.0;
        double renderResidualTime = 0.0;
        
        float timeScale = 1.0f;      // 1.0 = Normal, 2.0 = Fast Forward, 0.5 = Slow Motion
        double targetUPS = 60.0;   // Taxa real do NES NTSC
        double targetFPS = 60.0;     // Taxa de renderização desejada

        // Cálculo de FPS real
        float currentFPS = 0.0f;
        int frameCount = 0;
        float fpsTimer = 0.0f;

        // Flag para ignorar o limite de tempo (Fast Forward ilimitado)
        bool uncappedSpeed = false;
    };
}