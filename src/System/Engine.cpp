#include "Engine.h"
#include <SDL.h>
#include <iostream>
#include <sstream>
#include <cmath>

namespace R2NES::Core
{
    // Cria os componentes centrais e conclui sua configuração inicial.
    Engine::Engine()
    {
        // Inicializa os componentes principais
        window = std::make_unique<Window>("R2NES v2", 256, 240, 1);
        nes = std::make_unique<NES>();
        audioManager = std::make_unique<R2NES::System::AudioManager>();
        inputManager = std::make_unique<R2NES::System::InputManager>();
        stateManager = std::make_unique<R2NES::System::GameStateManager>();

        this->init();
    }

    // A destruição dos componentes é feita automaticamente pelos smart pointers.
    Engine::~Engine()
    {
    }

    // Conecta a interface aos subsistemas e aplica as opções atuais ao NES.
    void Engine::init()
    {
        // Cria o menu
        window->createMenu();

        // ----------------------  Callbacks ------------------------------------//
        // Conecta o callback da janela à função da Engine
        window->setKeyCallback([this](SDL_Keycode key, bool isPressed)
                               { this->handleKeyboard(key, isPressed); });

        // Conecta o callback de controle
        window->setControllerCallback([this](int player, SDL_GameControllerButton button, bool isPressed)
                                      { this->inputManager->handleJoystick(player, button, isPressed, *nes); });

        // Conecta o callback de gatilhos do controle
        window->setControllerTriggerCallback([this](int player, SDL_GameControllerAxis axis, bool isPressed)
                                             { this->handleJoystickTrigger(player, axis, isPressed, *nes); });

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
                                      { this->inputManager->configureABBAButtons(enabled); });

        window->setUseZapperCallback([this](bool enabled)
                                     { this->inputManager->configureUseZapper(enabled, *nes); });

        window->setTilesCallback([this](bool enabled)
                                 {
            this->tilesEnabled = enabled;
            if (nes) nes->setTilesEnabled(enabled); });

        window->setSpritesCallback([this](bool enabled)
                                   {
            this->spritesEnabled = enabled;
            if (nes) nes->setSpritesEnabled(enabled); });

        // Conecta o callback de FF
        window->setFFCallback([this](bool enabled)
                              { this->fastForwardEnabled = enabled; });

        // Conecta o callback de Rewind
        window->setRewindCallback([this](bool enabled)
                                  { this->rewindEnabled = enabled; });

        window->setCPUOverclockCallback([this](bool enabled)
                                        { 
                                            this->cpuOverclockEnabled = enabled; 
                                            if (nes) nes->setCPUOverclock(enabled); });

        // Conecta o callback de Pause
        window->setPauseCallback([this](bool p)
                                 { this->paused = p; });

        // Verifica os estados iniciais das opções
        this->vsyncEnabled = window->isVSyncEnabled();
        this->unlimitedSprites = window->isUnlimitedSpritesEnabled();
        this->fastForwardEnabled = window->isFastForwardEnabled();
        this->rewindEnabled = window->isRewindEnabled();
        this->soundEnabled = window->isSoundEnabled();
        this->cpuOverclockEnabled = window->isCPUOverclockEnabled();

        // Sincroniza o estado inicial do Overclock
        nes->setCPUOverclock(this->cpuOverclockEnabled);

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

        // Sincroniza o estado inicial de renderização
        nes->setTilesEnabled(this->tilesEnabled);
        nes->setSpritesEnabled(this->spritesEnabled);

        nes->getApu().setAudioSampleRate(static_cast<float>(audioManager->getSampleRate()));
    }

    // Delega a mudança de VSync à janela, que atualiza o estado pelo callback registrado.
    void Engine::toggleVSync()
    {
        // A Engine solicita a mudança para a Window
        // O callback configurado no construtor atualizará o vsyncEnabled da Engine
        // garantindo que ambos fiquem sincronizados.
        window->toggleVSync();
    }

    // Direciona o evento de gatilho do gamepad ao controlador emulado do jogador correto.
    void Engine::handleJoystickTrigger(int playerNum, SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes)
    {
        if (playerNum == 1)
        {
            handleJoystickTrigger1(axis, isPressed, nes);
        }
        else if (playerNum == 2)
        {
            handleJoystickTrigger2(axis, isPressed, nes);
        }
    }

    // Atualiza exclusivamente os gatilhos do primeiro controle.
    void Engine::handleJoystickTrigger1(SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes)
    {
        auto &joy1 = nes.getJoysticks().controller1;
        if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
            this->setRewind(this->rewindEnabled && isPressed);
        }
        else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            this->setFastForward(this->fastForwardEnabled && isPressed);
        }
    }

    // Atualiza exclusivamente os gatilhos do segundo controle.
    void Engine::handleJoystickTrigger2(SDL_GameControllerAxis axis, bool isPressed, Core::NES &nes)
    {
        auto &joy2 = nes.getJoysticks().controller2;
        if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
            this->setRewind(this->rewindEnabled && isPressed);
        }
        else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            this->setFastForward(this->fastForwardEnabled && isPressed);
        }
    }

    // Mantém o ciclo de eventos, emulação e vídeo de acordo com o modo de execução ativo.
    void Engine::run()
    {
        // Usa o contador de alta resolução do SDL como referência do primeiro delta.
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
                if (runningRewind)
                {
                    if (updateRewind(deltaTime))
                        render();
                }
                else if (!paused)
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

                        // Limita atrasos acumulados para evitar muitas atualizações em um único ciclo.
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
                        // Preserva somente a fração excedente para manter a cadência de renderização.
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

    // Converte eventos da janela em entrada do emulador e comandos de gerenciamento da ROM.
    void Engine::processEmulatorInput()
    {
        window->pollEvents();

        const uint8_t *keyboardState = SDL_GetKeyboardState(nullptr);
        // Consulta contínua garante que o rewind pare ao soltar Caps Lock.
        //setRewind(rewindEnabled && keyboardState[SDL_SCANCODE_CAPSLOCK]);

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
            // Estados de outra ROM não podem ser restaurados na ROM recém-carregada.
            clearRewindStates();
            stateManager->loadRom(romPath, *nes, *window);
        }

        if (window->isResetRequested())
        {
            // O reset invalida os snapshots capturados antes dele.
            clearRewindStates();
            stateManager->reset(*nes, *window);
        }

        if (window->isUnloadRequested())
        {
            // Sem cartucho, nenhum estado de rewind deve permanecer disponível.
            clearRewindStates();
            stateManager->unloadRom(*nes, *window);
        }

        stateManager->handleSaveLoadState(*nes, *window);
    }

    // Encaminha teclas ao controle e executa atalhos de save, vídeo, pausa e velocidade.
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
            {
                clearRewindStates();
                nes->reset();
            }
            break;
        case SDLK_TAB:
            this->setFastForward(this->fastForwardEnabled && isPressed);
            break;
        case SDLK_CAPSLOCK:
            this->setRewind(this->rewindEnabled && isPressed);
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

    // Muda o fast-forward preservando e restaurando as opções anteriores de velocidade e VSync.
    void Engine::setFastForward(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (runningFastForward == enabled || !nes->isCartridgeLoaded())
            return;

        runningFastForward = enabled;

        if (runningFastForward)
        {
            // Guarda as preferências do usuário para restaurá-las ao soltar o atalho.
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

    // Controla o rewind e interrompe o áudio pendente ao começar a retroceder.
    void Engine::setRewind(bool enabled)
    {      
        // Se não houve mudança, não fazemos nada
        if (runningRewind == enabled || !nes->isCartridgeLoaded())
            return;

        runningRewind = enabled;
        rewindResidualTime = 0.0;

        if (runningRewind)
            audioManager->clearQueuedAudio();
    }

    // Salva snapshots espaçados do NES em uma fila com duração máxima configurada.
    void Engine::captureRewindState()
    {
        if (++framesSinceLastRewindState < rewindStateIntervalFrames[window->getRewindPrecisionLevel()])
            return;

        framesSinceLastRewindState = 0;

        std::ostringstream state(std::ios::binary | std::ios::out);
        if (!nes->saveState(state))
            return;

        const size_t maximumStates = static_cast<size_t>(
            std::ceil(rewindHistorySeconds * targetUPS / rewindStateIntervalFrames[window->getRewindPrecisionLevel()]));
        if (maximumStates == 0)
            return;

        if (rewindStates.size() == maximumStates)
            rewindStates.pop_front();

        rewindStates.push_back(state.str());
    }

    // Restaura um snapshot recente quando o intervalo de rewind permite uma nova etapa.
    bool Engine::updateRewind(double deltaTime)
    {
        rewindResidualTime += deltaTime;
        if (rewindResidualTime < rewindIntervalSeconds[window->getRewindPrecisionLevel()] || rewindStates.empty())
            return false;

        // Mantém apenas o excedente de tempo para preservar o ritmo do rewind.
        rewindResidualTime = std::fmod(rewindResidualTime, rewindIntervalSeconds[window->getRewindPrecisionLevel()]);
        std::istringstream state(rewindStates.back(), std::ios::binary | std::ios::in);
        if (nes->loadState(state))
        {
            rewindStates.pop_back();
            return true;
        }

        return false;
    }

    // Remove todos os snapshots e reinicia os acumuladores do rewind.
    void Engine::clearRewindStates()
    {
        rewindStates.clear();
        framesSinceLastRewindState = 0;
        rewindResidualTime = 0.0;
    }

    // Executa um quadro ou instrução, encaminha o áudio produzido e registra rewind.
    void Engine::update()
    {
        inputManager->update(*nes, frameCount);

        if (stepByStep)
        {
            if (stepRequested)
            {
                nes->step(false);
                // Em modo step, o áudio geralmente é ignorado ou produz "clicks"
                while (!nes->getCpu().complete())
                {
                    nes->step(false);
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

            captureRewindState();
        }

        if (!uncappedSpeed)
            frameCount++; // Conta quadros emulados no modo normal
    }

    // Exibe o framebuffer e alimenta os visualizadores auxiliares que estiverem abertos.
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