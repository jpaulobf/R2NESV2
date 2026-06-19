#include "Window/Engine.h"
#include <SDL.h>
#include <iostream>

namespace R2NES::Core
{
    Engine::Engine()
    {
        // Inicializa os componentes principais
        window = std::make_unique<Window>("R2NES v2", 256, 240, 1);
        window->createMenu();
        nes = std::make_unique<NES>();

        audioManager = std::make_unique<R2NES::System::AudioManager>();
        inputManager = std::make_unique<R2NES::System::InputManager>();
        stateManager = std::make_unique<R2NES::System::GameStateManager>();

        // Conecta o callback da janela à função da Engine
        window->setKeyCallback([this](SDL_Keycode key, bool isPressed)
                               { this->handleKeyboard(key, isPressed); });

        // Conecta o callback de controle
        window->setControllerCallback([this](int player, SDL_GameControllerButton button, bool isPressed)
                                      { this->inputManager->handleJoystick(player, button, isPressed, *nes); });

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
                                             audioManager->clearQueuedAudio();
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

        window->setInvertBAYBCallback([this](bool enabled)
                                      { 
                                         this->inputManager->configureABBAButtons(enabled); });

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

        // Inicializa o estado da PPU com a configuração da janela
        nes->getPpu().setUnlimitedSprites(this->unlimitedSprites);
        
        nes->getApu().setAudioSampleRate(static_cast<float>(audioManager->getSampleRate()));
    }

    void Engine::toggleVSync()
    {
        // A Engine solicita a mudança para a Window
        // O callback configurado no construtor atualizará o vsyncEnabled da Engine
        // garantindo que ambos fiquem sincronizados.
        window->toggleVSync();
    }

    Engine::~Engine()
    {
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

            // Só processa o timing e a atualização se houver um cartucho carregado no NES
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

        std::string romPath = window->getSelectedPath();
        if (!romPath.empty())
        {
            stateManager->loadRom(romPath, *nes, *window);
        }

        if (window->isResetRequested())
        {
            stateManager->reset(*nes, *window);
        }

        if (window->isUnloadRequested())
        {
            stateManager->unloadRom(*nes, *window);
        }

        stateManager->handleSaveLoadState(*nes, *window);
    }



    void Engine::handleKeyboard(SDL_Keycode key, bool isPressed)
    {
        inputManager->handleKeyboard(key, isPressed, *nes);

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
        inputManager->update(*nes, frameCount);

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
                audioManager->pushSample(nes->getApu().getOutputSample());
            }

            // 3. Envia o buffer de áudio do frame inteiro para o SDL
            audioManager->queueAudio(uncappedSpeed);
        }

        if (!uncappedSpeed)
            frameCount++; // Conta quadros emulados no modo normal
    }

    void Engine::render()
    {
        // Usamos o disassembly já armazenado e o PC atual da CPU
        auto &cpu = nes->getCpu();
        uint16_t currentPC = cpu.pc;

        // Renderiza apenas a tela do NES e o FPS
        window->render(nes->getPpu().getFrameBuffer(), currentFPS);

        // Se o OAM Viewer estiver aberto, envia os dados da PPU
        if (window->isOamViewerOpen() && nes->isCartridgeLoaded())
        {
            window->updateOamViewer(nes->getPpu().getOamMemory());
        }

        // Se o Disassembler estiver aberto, verifica se precisamos atualizar o cache por causa de bank switch
        if (window->isDisassemblerOpen() && nes->isCartridgeLoaded())
        {
            stateManager->updateDisassemblyCache(*nes, currentPC);
        }

        // Se o VRAM Viewer estiver aberto, envia os dados da PPU
        if (window->isVramViewerOpen() && nes->isCartridgeLoaded())
        {
            window->updateVramViewer(&nes->getPpu().getVram());
        }

        // Se o Disassembler estiver aberto, atualiza-o
        if (window->isDisassemblerOpen())
        {
            window->updateDisassembler(currentPC, stateManager->getCachedDisassembly(), stepByStep, stepRequested, cpu.a, cpu.x, cpu.y, cpu.stkp, cpu.status);
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