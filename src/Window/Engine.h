#pragma once

#include "Window.h"
#include "Core/NESBoard.h"
#include <memory>

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
        void processInput();
        void update();
        void render();

        std::unique_ptr<Window> window;
        std::unique_ptr<NesBoard> nes;

        bool isRunning = true;

        // Variáveis de controle de tempo
        double residualTime = 0.0;
    };
}