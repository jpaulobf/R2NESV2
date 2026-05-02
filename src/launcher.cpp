#include <iostream>
#include <SDL.h>
#include "Core/NESBoard.h"
#include "Window/Window.h"

int main(int argc, char* argv[])
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;

    R2NES::Core::NesBoard nes;

    // A resolução interna do NES é 256x240. 
    // Usamos escala 3 para que a janela não fique muito pequena.
    R2NES::Core::Window window("R2NES v2", 256, 240, 3);
    window.createMenu();

    // Loop principal da aplicação
    while (!window.shouldClose())
    {
        // 1. Processa eventos (teclado, fechar janela, etc)
        window.pollEvents();

        // 2. Verifica se uma ROM foi selecionada via menu
        std::string romPath = window.getSelectedPath();
        if (!romPath.empty())
        {
            std::cout << "Loading ROM: " << romPath << std::endl;
            nes.insertCartridge(romPath);
            nes.reset();
            window.clearSelectedPath();
        }

        // 3. Renderiza o frame atual da PPU
        window.render(nes.getPpu().getFrameBuffer());
    }

    return 0;
}