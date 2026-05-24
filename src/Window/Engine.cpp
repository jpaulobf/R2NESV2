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

        // Conecta o callback de VSync para sincronizar o loop da Engine
        window->setVSyncCallback([this](bool enabled)
                                 { this->vsyncEnabled = enabled; });

        window->setUnlimitedSpritesCallback([this](bool enabled)
                                            { 
                                                this->unlimitedSprites = enabled; 
                                                if (nes) 
                                                    nes->getPpu().setUnlimitedSprites(enabled); });

        // Conecta o callback de FF
        window->setFFCallback([this](bool enabled)
                              { this->fastForwardEnabled = enabled; });

        this->vsyncEnabled = window->isVSyncEnabled();
        this->unlimitedSprites = window->isUnlimitedSpritesEnabled();
        this->fastForwardEnabled = window->isFastForwardEnabled();

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

        // Inicializa o estado da PPU com a configuração da janela
        nes->getPpu().setUnlimitedSprites(this->unlimitedSprites);

        // Inicialização do Áudio SDL
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = 44100;
        want.format = AUDIO_F32SYS;
        want.channels = 1;
        want.samples = 1024;

        audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (audioDevice > 0)
        {
            std::cout << "Audio: Device opened successfully (ID: " << audioDevice << ")" << std::endl;
            SDL_PauseAudioDevice(audioDevice, 0);
            nes->getApu().setAudioSampleRate(static_cast<float>(have.freq));
            // Opcional: ajustar o tempo de slew aqui se desejar
            // nes->getApu().setSlewMs(0.5f);
        }
        else
        {
            std::cerr << "Audio: Failed to open device! SDL_Error: " << SDL_GetError() << std::endl;
        }

        // Inicializa a última amostra da APU (usa o estado inicial do APU)
        lastApuSample = nes->getApu().getOutputSample();
    }

    void Engine::toggleVSync()
    {
        // A Engine solicita a mudança para a Window
        // O callback configurado no construtor atualizará o vsyncEnabled da Engine
        // garantindo que ambos fiquem sincronizados.
        window->toggleVSync();
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

    Engine::~Engine()
    {
        if (audioDevice > 0)
            SDL_CloseAudioDevice(audioDevice);
    }

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
                    // std::cout << "Uncapped Speed Enabled" << std::endl;

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
                else if (vsyncEnabled) // uncapped tem prioridade sobre vsync
                {
                    // std::cout << "Vsync Enabled" << std::endl;
                    update();
                    render();
                }
                else
                {
                    // Lógica unificada para VSync e Modo Normal
                    // Isso garante que o NES rode a 60 UPS (Updates Per Second)
                    // independentemente da taxa de atualização do monitor (75Hz, 144Hz, etc)
                    double updateInterval = 1.0 / targetUPS;
                    residualTime += deltaTime * timeScale;

                    if (residualTime > 0.1f)
                        residualTime = 0.1f;

                    while (residualTime >= updateInterval - 0.0002)
                    {
                        update();
                        residualTime -= updateInterval;
                    }

                    if (vsyncEnabled)
                    {
                        render();
                    }
                    else
                    {
                        double renderInterval = 1.0 / targetFPS;
                        renderResidualTime += deltaTime;

                        if (renderResidualTime >= renderInterval)
                        {
                            render();
                            renderResidualTime = std::fmod(renderResidualTime, renderInterval);
                        }
                    }
                }
            }
            else
            {
                // Se não há jogo, apenas renderiza a interface (ImGui/Menu)
                render();
            }
        }
    }

    void Engine::processEmulatorInput()
    {
        window->pollEvents();

        // Suporte à Zapper: Passa a posição da mira (mouse) e o estado do gatilho para o hardware
        if (nes->isCartridgeLoaded())
        {
            auto mouse = window->getMouseState();
            nes->getPpu().setZapperPos(mouse.x, mouse.y);
            nes->getBus().setZapperTrigger(mouse.leftButton);
        }

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
            break;
        case SDLK_F8:
            if (isPressed)
                window->windowResize(2);
            break;
        case SDLK_F9:
            if (isPressed)
                window->windowResize(3);
            break;
        case SDLK_F10:
            if (isPressed)
                window->windowResize(4);
            break;
        case SDLK_F11:
            if (isPressed)
                window->windowBorderlessFullscreen();
            break;
        case SDLK_F12:
            if (isPressed)
                nes->reset();
            break;
        case SDLK_TAB:
            // Ao pressionar tab, alterna para FF se FFEnabled for True, caso contrário, retorna ao estado normal.
            this->setFastForward(this->fastForwardEnabled && isPressed);
            break;
        }
    }

    void Engine::setFastForward(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (runningFastForward == enabled)
            return;

        runningFastForward = enabled;

        if (runningFastForward)
        {
            oldUncappedSpeed = uncappedSpeed;
            oldVsyncEnabled = vsyncEnabled;
            uncappedSpeed = true;
            vsyncEnabled = false;
        }
        else
        {
            uncappedSpeed = oldUncappedSpeed;
            vsyncEnabled = oldVsyncEnabled;
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

        // Sincronização de Áudio: CPU freq / Sample rate (1789773 / 44100 = 40.58)
        const double cyclesPerSample = 1789773.0 / 44100.0;

        if (stepByStep)
        {
            if (stepRequested)
            {
                nes->step();
                // Em modo step, o áudio geralmente é ignorado ou produz "clicks"
                while (!nes->getCpu().complete())
                {
                    nes->step();
                }
                stepRequested = false;
            }
        }
        else
        {
            std::vector<float> samples;
            while (!nes->isFrameComplete())
            {
                double prevAcc = audioCycleAccumulator;
                nes->step();

                // Incrementa o acumulador de ciclos (1 ciclo da CPU por step)
                audioCycleAccumulator += 1.0;

                // Reduzimos o ganho master para evitar clipping após o mixer
                float masterGain = 2.0f;

                // Amostra atual da APU (após o passo)
                float currentApuSample = nes->getApu().getOutputSample();

                // Se cruzou o limiar para gerar uma amostra, interpola linearmente
                // entre a última amostra e a amostra atual usando a fração do ciclo.
                if (audioCycleAccumulator >= cyclesPerSample)
                {
                    double fraction = (cyclesPerSample - prevAcc); // no intervalo (0,1]
                    if (fraction < 0.0)
                        fraction = 0.0;
                    if (fraction > 1.0)
                        fraction = 1.0;
                    float sample = (lastApuSample * static_cast<float>(1.0 - fraction) + currentApuSample * static_cast<float>(fraction)) * masterGain;
                    samples.push_back(sample);
                    audioCycleAccumulator -= cyclesPerSample;
                }

                // Atualiza a última amostra para próxima interpolação
                lastApuSample = currentApuSample;
            }
            nes->clearFrameComplete();

            // Envia o buffer de áudio do frame para o SDL
            if (audioDevice > 0 && !samples.empty())
            {
                // Se o buffer do SDL estiver muito cheio (mais de 0.2s), limpa para evitar lag
                if (SDL_GetQueuedAudioSize(audioDevice) > 44100 * sizeof(float) / 5)
                {
                    // Opcional: SDL_ClearQueuedAudio(audioDevice);
                }

                // Enfileira o áudio se não houver excesso
                if (SDL_GetQueuedAudioSize(audioDevice) < 44100 * sizeof(float) / 4)
                {
                    SDL_QueueAudio(audioDevice, samples.data(), samples.size() * sizeof(float));
                }
            }
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

        // Se o Disassembler estiver aberto, atualiza-o
        if (window->isDisassemblerOpen())
        {
            window->updateDisassembler(currentPC, cachedDisassembly, stepByStep, stepRequested, cpu.a, cpu.x, cpu.y, cpu.stkp, cpu.status);
        }

        // Se o Tile Viewer estiver aberto, gera os dados e envia para a janela secundária
        if (window->isTileViewerOpen() && nes->isCartridgeLoaded())
        {
            auto p0 = nes->getPpu().getPatternTablePixels(0, 0);
            auto p1 = nes->getPpu().getPatternTablePixels(1, 0);
            window->updateTileViewer(p0.data(), p1.data());
        }

        // Se o RamViewer estiver aberto, atualiza-o
        if (window->isRamViewerOpen() && nes->isCartridgeLoaded())
        {
            window->updateRamViewer(nes->getBus().ram);
        }
    }
}