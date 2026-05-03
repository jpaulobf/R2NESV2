#include "Window/Engine.h"
#include <SDL.h>
#include <iostream>
#include <map>

namespace R2NES::Core
{
    Engine::Engine()
    {
        // Inicializa os componentes principais
        window = std::make_unique<Window>("R2NES v2", 256, 240, 3);
        window->createMenu();
        nes = std::make_unique<NesBoard>();
    }

    Engine::~Engine() {}

    void Engine::run()
    {
        uint32_t lastTime = SDL_GetTicks();

        while (!window->shouldClose() && isRunning)
        {
            uint32_t currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;

            processEmulatorInput();

            // Só processa o timing e a atualização se houver um cartucho carregado no NesBoard
            if (nes->isCartridgeLoaded())
            {
                // O NES precisa de um timing preciso.
                // Um frame de NES dura aproximadamente 1/60 segundos.
                residualTime += deltaTime;
                while (residualTime >= 1.0f / 60.0f)
                {
                    update();
                    residualTime -= 1.0f / 60.0f;
                }
            }

            render();
        }
    }

    void Engine::processEmulatorInput()
    {
        window->pollEvents();

        // Verifica se uma ROM foi selecionada via menu
        std::string romPath = window->getSelectedPath();
        if (!romPath.empty())
        {
            std::cout << "Engine: Loading ROM -> " << romPath << std::endl;
            nes->insertCartridge(romPath);
            nes->reset();

            // Gera o disassembly apenas uma vez no carregamento
            cachedDisassembly = nes->getCpu().disassemble(0x8000, 0xFFFF);
            
            window->clearSelectedPath();
        }
    }

    void Engine::update()
    {
        // Um frame de NES consome aproximadamente 29780 ciclos de CPU.
        // O ideal é rodar o NesBoard até que um frame completo seja gerado.
        // Se o seu NesBoard tiver um método que rode até o próximo VBlank, use-o aqui.
        while (!nes->isFrameComplete())
        {
            nes->step();
        }
        nes->clearFrameComplete();
    }

    void Engine::render()
    {
        // Usamos o disassembly já armazenado e o PC atual da CPU
        uint16_t currentPC = nes->getCpu().pc;

        // Pega o buffer de pixels da PPU e manda para a Window
        window->render(nes->getPpu().getFrameBuffer(), currentPC, cachedDisassembly);

        // Se o Tile Viewer estiver aberto, gera os dados e envia para a janela secundária
        if (window->isTileViewerOpen() && nes->isCartridgeLoaded())
        {
            auto p0 = nes->getPpu().getPatternTablePixels(0, 0);
            auto p1 = nes->getPpu().getPatternTablePixels(1, 0);
            window->updateTileViewer(p0.data(), p1.data());
        }
    }
}