#include "Window/Engine.h"
#include <SDL.h>
#include <iostream>
#include <map>
#include <filesystem>
#include <fstream>

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

        window->setPaletteCallback([this](PaletteType preset)
                                   { this->nes->getPpu().setSystemPalette(preset); });

        // Conecta o callback de Sound para sincronizar o estado do som
        window->setSoundCallback([this](bool enabled)
                                 { 
                                     this->soundEnabled = enabled; 
                                     if (nes) 
                                     {
                                         if (enabled)
                                             nes->getApu().enableSound();
                                         else
                                         {
                                             nes->getApu().disableSound();
                                             if (audioDevice > 0) 
                                                 SDL_ClearQueuedAudio(audioDevice);
                                         }
                                     } });

        // Conecta os callbacks dos canais individuais da APU
        window->setPulse1Callback([this](bool enabled)
                                  { 
            if (nes) nes->getApu().setPulse1Enabled(enabled); });
        window->setPulse2Callback([this](bool enabled)
                                  { 
            if (nes) nes->getApu().setPulse2Enabled(enabled); });
        window->setTriangleCallback([this](bool enabled)
                                    { 
            if (nes) nes->getApu().setTriangleEnabled(enabled); });
        window->setNoiseCallback([this](bool enabled)
                                 { 
            if (nes) nes->getApu().setNoiseEnabled(enabled); });
        window->setDMCCallback([this](bool enabled)
                               { 
            if (nes) nes->getApu().setDMCEnabled(enabled); });

        window->setUnlimitedSpritesCallback([this](bool enabled)
                                            { 
                                                this->unlimitedSprites = enabled; 
                                                if (nes) 
                                                    nes->getPpu().setUnlimitedSprites(enabled); });

        // Conecta o callback de FF
        window->setFFCallback([this](bool enabled)
                              { this->fastForwardEnabled = enabled; });

        // Conecta o callback de Pause
        window->setPauseCallback([this](bool p)
                                 { this->paused = p; });

        this->vsyncEnabled = window->isVSyncEnabled();
        this->unlimitedSprites = window->isUnlimitedSpritesEnabled();
        this->fastForwardEnabled = window->isFastForwardEnabled();
        this->soundEnabled = window->isSoundEnabled();

        // Sincroniza o estado inicial da APU
        if (this->soundEnabled)
            nes->getApu().enableSound();
        else
            nes->getApu().disableSound();

        // Sincroniza o estado inicial dos canais individuais
        nes->getApu().setPulse1Enabled(window->isPulse1Enabled());
        nes->getApu().setPulse2Enabled(window->isPulse2Enabled());
        nes->getApu().setTriangleEnabled(window->isTriangleEnabled());
        nes->getApu().setNoiseEnabled(window->isNoiseEnabled());
        nes->getApu().setDMCEnabled(window->isDMCEnabled());

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
        want.samples = 512;

        audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (audioDevice > 0)
        {
            std::cout << "Audio: Device opened successfully (ID: " << audioDevice << ")" << std::endl;
            SDL_PauseAudioDevice(audioDevice, 0);
            nes->getApu().setAudioSampleRate(static_cast<float>(have.freq));
        }
        else
        {
            std::cerr << "Audio: Failed to open device! SDL_Error: " << SDL_GetError() << std::endl;
        }
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
                if (!paused)
                {
                    if (stepByStep)
                    {
                        update();
                    }
                    else if (uncappedSpeed)
                    {
                        // Emulação (UPS)
                        for (int i = 0; i < 10; ++i)
                        {
                            update();
                            frameCount++;
                        }
                    }
                    else if (vsyncEnabled)
                    {
                        update();
                    }
                    else
                    {
                        // Lógica de tempo normal
                        double updateInterval = 1.0 / targetUPS;
                        residualTime += deltaTime * timeScale;

                        if (residualTime > 0.1f)
                            residualTime = 0.1f;

                        while (residualTime >= updateInterval - 0.0002)
                        {
                            update();
                            residualTime -= updateInterval;
                        }
                    }
                }

                if (uncappedSpeed)
                {
                    renderResidualTime += deltaTime;
                    if (renderResidualTime >= 1.0 / targetFPS)
                    {
                        render();
                        renderResidualTime = 0;
                    }
                }
                else if (vsyncEnabled)
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
            else
            {
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

            this->currentRomPath = romPath;
            std::cout << "Engine: Loading ROM -> " << romPath << std::endl;
            nes->insertCartridge(romPath);
            nes->reset();
            window->setPaused(false); // Garante que comece despausado ao carregar novo jogo

            // Gera o disassembly apenas uma vez no carregamento
            cachedDisassembly = nes->getCpu().disassemble(0x8000, 0xFFFF);

            window->clearSelectedPath();
            window->setCartLoaded(true); // Informa a janela que um cartucho foi carregado para habilitar opções dependentes disso
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
            window->setCartLoaded(false); // Informa a janela que o cartucho foi descarregado para desabilitar opções dependentes disso
        }

        // Lógica de Save/Load State
        if (nes->isCartridgeLoaded())
        {
            if (window->getIsToSave() || window->getIsToLoad())
            {
                namespace fs = std::filesystem;

                // 1. Prepara o nome do arquivo: [rom].[slot].sav
                std::string romName = fs::path(currentRomPath).stem().string();
                std::string slot = std::to_string(window->getSaveSlot());

                fs::create_directories("savestates"); // Garante que a pasta existe
                std::string filename = "savestates/" + romName + "." + slot + ".sav";

                if (window->getIsToSave())
                {
                    if (nes->saveState(filename))
                        std::cout << "Engine: State saved to " << filename << std::endl;
                }
                else if (window->getIsToLoad())
                {
                    if (nes->loadState(filename))
                        std::cout << "Engine: State loaded from " << filename << std::endl;
                }

                window->resetSaveLoadFlags();
            }
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
        case SDLK_F5:
            if (isPressed && nes->isCartridgeLoaded())
            {
                window->setLoad(false);
                window->setSave(true);
            }
            break;
        case SDLK_F6:
            if (isPressed && nes->isCartridgeLoaded())
            {
                window->setSave(false);
                window->setLoad(true);
            }
            break;
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
            this->setFastForward(this->fastForwardEnabled && isPressed);
            break;
        case SDLK_p:
        case SDLK_PAUSE:
            if (isPressed && nes->isCartridgeLoaded())
            {
                window->setPaused(!window->isPaused());
            }
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

            // 1. Roda a CPU e a APU até o final do quadro de vídeo (Frame)
            while (!nes->isFrameComplete())
            {
                nes->step();
                // A cada step, a APU agora gera e guarda as amostras de áudio sozinha!
            }
            nes->clearFrameComplete();

            // 2. Coleta todo o áudio que a APU gerou durante este frame
            while (nes->getApu().hasSamples())
            {
                samples.push_back(nes->getApu().getOutputSample());
            }

            // 3. Envia o buffer de áudio do frame inteiro para o SDL
            if (audioDevice > 0 && !samples.empty())
            {
                if (!uncappedSpeed)
                {
                    // Latência alvo de ~3 frames (~50ms)
                    Uint32 maxSafeBytes = 44100 * sizeof(float) / 20;

                    // Se o buffer engasgar e acumular áudio velho, limpamos
                    if (SDL_GetQueuedAudioSize(audioDevice) > maxSafeBytes)
                    {
                        SDL_ClearQueuedAudio(audioDevice);
                    }

                    SDL_QueueAudio(audioDevice, samples.data(), samples.size() * sizeof(float));
                }
                else
                {
                    // Fast-Forward
                    SDL_ClearQueuedAudio(audioDevice);
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

        // Se o OAM Viewer estiver aberto, envia os dados da PPU
        if (window->isOamViewerOpen() && nes->isCartridgeLoaded())
        {
            window->updateOamViewer(nes->getPpu().getOamMemory());
        }

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

        // Se o Palette Viewer estiver aberto, envia os dados da PPU
        if (window->isPaletteViewerOpen())
        {
            window->updatePaletteViewer(nes->getPpu().getPaletteTable(), nes->getPpu().getSystemPalette());
        }

        // Se o RamViewer estiver aberto, atualiza-o
        if (window->isRamViewerOpen() && nes->isCartridgeLoaded())
        {
            window->updateRamViewer(nes->getBus().ram);
        }
    }
}