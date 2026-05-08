#include "Window/Engine.h"
#include <SDL.h>
#include <iostream>
#include <map>

namespace R2NES::Core
{
    Engine::Engine()
    {
        // Inicializa os componentes principais
        window = std::make_unique<Window>("R2NES v2", 256, 240, 1);
        window->createMenu();
        nes = std::make_unique<NesBoard>();

        // Conecta o callback da janela à função da Engine
        window->setKeyCallback([this](SDL_Keycode key, bool isPressed)
                               { this->handleKeyboard(key, isPressed); });

        // Conecta o callback de controle
        window->setControllerCallback([this](int player, SDL_GameControllerButton button, bool isPressed)
                                      { this->handleJoystick(player, button, isPressed); });

        // Inicializa o mapeamento de teclas padrão para o Player 1
        player1KeyMap[SDLK_j] = R2NES::Core::IO::BUTTON_B;
        player1KeyMap[SDLK_k] = R2NES::Core::IO::BUTTON_A;
        player1KeyMap[SDLK_BACKSPACE] = R2NES::Core::IO::BUTTON_SELECT;
        player1KeyMap[SDLK_RETURN] = R2NES::Core::IO::BUTTON_START;
        player1KeyMap[SDLK_w] = R2NES::Core::IO::BUTTON_UP;
        player1KeyMap[SDLK_s] = R2NES::Core::IO::BUTTON_DOWN;
        player1KeyMap[SDLK_a] = R2NES::Core::IO::BUTTON_LEFT;
        player1KeyMap[SDLK_d] = R2NES::Core::IO::BUTTON_RIGHT;

        // Mapeamento de Turbo (Teclado)
        player1TurboKeyMap[SDLK_i] = R2NES::Core::IO::BUTTON_A;
        player1TurboKeyMap[SDLK_u] = R2NES::Core::IO::BUTTON_B;

        // Mapeamento Padrão para Controles (Xbox/8BitDo layout)
        // Player 1
        player1ControllerMap[SDL_CONTROLLER_BUTTON_A] = R2NES::Core::IO::BUTTON_A;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_B] = R2NES::Core::IO::BUTTON_B;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_BACK] = R2NES::Core::IO::BUTTON_SELECT;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_START] = R2NES::Core::IO::BUTTON_START;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_UP] = R2NES::Core::IO::BUTTON_UP;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = R2NES::Core::IO::BUTTON_DOWN;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = R2NES::Core::IO::BUTTON_LEFT;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = R2NES::Core::IO::BUTTON_RIGHT;

        // Mapeamento de Turbo (Controle)
        player1TurboControllerMap[SDL_CONTROLLER_BUTTON_Y] = R2NES::Core::IO::BUTTON_A;
        player1TurboControllerMap[SDL_CONTROLLER_BUTTON_X] = R2NES::Core::IO::BUTTON_B;

        // Player 2 (mesmo mapeamento, controles diferentes)
        player2ControllerMap = player1ControllerMap;
    }

    void Engine::handleJoystick(int playerNum, SDL_GameControllerButton button, bool isPressed)
    {
        if (playerNum == 1)
        {
            handleJoystick1(button, isPressed);
        }
        else if (playerNum == 2)
        {
            handleJoystick2(button, isPressed);
        }
    }

    Engine::~Engine() {}

    void Engine::run()
    {
        uint64_t lastTime = SDL_GetPerformanceCounter();
        uint64_t frequency = SDL_GetPerformanceFrequency();

        while (!window->shouldClose() && isRunning)
        {
            uint64_t currentTime = SDL_GetPerformanceCounter();
            // deltaTime calculado com precisão de micro/nanosegundos
            double deltaTime = static_cast<double>(currentTime - lastTime) / frequency;
            lastTime = currentTime;

            processEmulatorInput();

            // Calcula o FPS real a cada segundo
            fpsTimer += static_cast<float>(deltaTime);
            if (fpsTimer >= 1.0f)
            {
                currentFPS = static_cast<float>(frameCount) / fpsTimer;
                frameCount = 0;
                fpsTimer = 0.0f;
            }

            // Só processa o timing e a atualização se houver um cartucho carregado no NesBoard
            if (nes->isCartridgeLoaded())
            {
                if (stepByStep)
                {
                    update();
                    render();
                }
                else if (uncappedSpeed)
                {
                    // Emulação (UPS) - Roda o máximo que o núcleo da CPU permitir
                    for (int i = 0; i < 10; ++i)
                    { // Pequeno batch para reduzir overhead do loop
                        update();
                        frameCount++; // Agora contamos quadros emulados
                    }

                    // Renderização (FPS) - Limitamos a renderização para não travar na GPU
                    renderResidualTime += deltaTime;
                    if (renderResidualTime >= 1.0 / targetFPS)
                    {
                        render();
                        renderResidualTime = 0;
                    }
                }
                else
                {
                    // 1. Emulação (UPS): Depende do timeScale
                    double updateInterval = 1.0 / targetUPS;
                    residualTime += deltaTime * timeScale;

                    // Evita a "espiral da morte" se o emulador estiver muito lento
                    if (residualTime > 0.1f)
                        residualTime = 0.1f;

                    while (residualTime >= updateInterval)
                    {
                        update();
                        residualTime -= updateInterval;
                    }

                    // 2. Renderização (FPS): Independente do timeScale
                    double renderInterval = 1.0 / targetFPS;
                    renderResidualTime += deltaTime;

                    if (renderResidualTime >= renderInterval)
                    {
                        render();
                        // No modo normal, contamos aqui ou no update.
                        // Vamos contar no update() para ser consistente.
                        renderResidualTime = std::fmod(renderResidualTime, renderInterval);
                    }
                }
            }
            else
            {
                // Se não há jogo, apenas renderiza a interface (ImGui/Menu)
                render();
                // frameCount++;
            }
        }
    }

    void Engine::processEmulatorInput()
    {
        window->pollEvents();

        // Verifica se uma ROM foi selecionada via menu
        std::string romPath = window->getSelectedPath();
        if (!romPath.empty())
        {
            // Garante que o sistema seja descarregado e limpo antes de carregar a nova ROM
            nes->unload();
            cachedDisassembly.clear();

            std::cout << "Engine: Loading ROM -> " << romPath << std::endl;
            nes->insertCartridge(romPath);
            nes->reset();

            // Gera o disassembly apenas uma vez no carregamento
            cachedDisassembly = nes->getCpu().disassemble(0x8000, 0xFFFF);

            window->clearSelectedPath();
        }

        // Verifica se o usuário clicou em "Reset" no menu
        if (window->isResetRequested())
        {
            std::cout << "Engine: Resetting NES..." << std::endl;
            nes->reset();
            window->clearResetRequest();
        }

        // Verifica se o usuário clicou em "Unload"
        if (window->isUnloadRequested())
        {
            std::cout << "Engine: Unloading ROM..." << std::endl;
            nes->unload();
            cachedDisassembly.clear(); // Limpa o cache do disassembler
            window->clearUnloadRequest();
        }
    }

    void Engine::handleJoystick1(SDL_GameControllerButton button, bool isPressed)
    {
        auto &joy1 = nes->getJoysticks().controller1;
        auto it = player1ControllerMap.find(button);
        if (it != player1ControllerMap.end())
        {
            joy1.setButton(it->second, isPressed);
        }

        // Verifica botões de Turbo no controle
        auto itTurbo = player1TurboControllerMap.find(button);
        if (itTurbo != player1TurboControllerMap.end())
        {
            if (itTurbo->second == R2NES::Core::IO::BUTTON_A)
                turboA = isPressed;
            if (itTurbo->second == R2NES::Core::IO::BUTTON_B)
                turboB = isPressed;

            // Garante que o botão seja solto no Core se o turbo for liberado
            if (!isPressed)
                joy1.setButton(itTurbo->second, false);
        }
    }

    void Engine::handleJoystick2(SDL_GameControllerButton button, bool isPressed)
    {
        auto &joy2 = nes->getJoysticks().controller2;
        auto it = player2ControllerMap.find(button);
        if (it != player2ControllerMap.end())
        {
            joy2.setButton(it->second, isPressed);
        }
    }

    void Engine::handleKeyboard(SDL_Keycode key, bool isPressed)
    {
        // Exemplo de mapeamento simples para o Controller 1
        auto &joy1 = nes->getJoysticks().controller1;

        // Procura a tecla no mapeamento do Player 1
        auto it = player1KeyMap.find(key);
        if (it != player1KeyMap.end())
        {
            joy1.setButton(it->second, isPressed);
        }

        // Verifica teclas de Turbo no teclado
        auto itTurbo = player1TurboKeyMap.find(key);
        if (itTurbo != player1TurboKeyMap.end())
        {
            if (itTurbo->second == R2NES::Core::IO::BUTTON_A)
                turboA = isPressed;
            if (itTurbo->second == R2NES::Core::IO::BUTTON_B)
                turboB = isPressed;

            if (!isPressed)
                joy1.setButton(itTurbo->second, false);
        }

        // Atalhos da Engine
        switch (key)
        {
        case SDLK_F7:
            if (isPressed)
                window->windowResize(1);
            break; //
        case SDLK_F8:
            if (isPressed)
                window->windowResize(2);
            break; //
        case SDLK_F9:
            if (isPressed)
                window->windowResize(3);
            break; //
        case SDLK_F10:
            if (isPressed)
                window->windowResize(4);
            break; //
        case SDLK_F11:
            if (isPressed)
                window->windowBorderlessFullscreen();
            break; //
        case SDLK_F12:
            if (isPressed)
                nes->reset();
            break; //
        }
    }

    void Engine::update()
    {
        // Lógica de Turbo: Oscila o estado dos botões a cada 2 frames (15Hz em 60FPS)
        // Isso garante que o jogo registre tanto o pressionamento quanto a liberação.
        auto &joy1 = nes->getJoysticks().controller1;
        bool turboPulse = (frameCount % 4 > 2); // Fica 'true' por 2 frames, 'false' por 2 frames

        if (turboA)
            joy1.setButton(R2NES::Core::IO::BUTTON_A, turboPulse);
        if (turboB)
            joy1.setButton(R2NES::Core::IO::BUTTON_B, turboPulse);

        if (stepByStep)
        {
            if (stepRequested)
            {
                nes->step();
                while (!nes->getCpu().complete())
                {
                    nes->step();
                }
                stepRequested = false;
            }
        }
        else
        {
            while (!nes->isFrameComplete())
            {
                nes->step();
            }
            nes->clearFrameComplete();
        }

        if (!uncappedSpeed)
            frameCount++; // Conta quadros emulados no modo normal
    }

    void Engine::render()
    {
        // Usamos o disassembly já armazenado e o PC atual da CPU
        auto &cpu = nes->getCpu();
        uint16_t currentPC = cpu.pc;

        // Pega o buffer de pixels da PPU e manda para a Window
        window->render(nes->getPpu().getFrameBuffer(), currentPC, cachedDisassembly, stepByStep, stepRequested,
                       cpu.a, cpu.x, cpu.y, cpu.stkp, cpu.status, currentFPS);

        // Se o Tile Viewer estiver aberto, gera os dados e envia para a janela secundária
        if (window->isTileViewerOpen() && nes->isCartridgeLoaded())
        {
            auto p0 = nes->getPpu().getPatternTablePixels(0, 0);
            auto p1 = nes->getPpu().getPatternTablePixels(1, 0);
            window->updateTileViewer(p0.data(), p1.data());
        }
    }
}