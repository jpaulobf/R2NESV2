#pragma once

#include "Window.h"
#include "Core/NESBoard.h"
#include <memory>
#include <map>
#include <string>

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
            void update();
            void render();

            std::unique_ptr<Window> window;
            std::unique_ptr<NesBoard> nes;

            // Cache para armazenar o código traduzido da ROM
            std::map<uint16_t, std::string> cachedDisassembly;

            bool stepByStep = false;
            bool stepRequested = false;

            bool isRunning = true;

            // Variáveis de controle de tempo
            double residualTime = 0.0;
    };
}