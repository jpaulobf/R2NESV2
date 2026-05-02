#pragma once

#include <SDL.h>
#include <string>
#include <cstdint>

namespace R2NES::Core
{
    class Window
    {
    public:
        Window(const std::string& title, int width, int height, int scale);
        ~Window();

        // Processa eventos do sistema (como o botão de fechar)
        void pollEvents();
        
        // Atualiza a textura com os pixels da PPU e desenha na tela
        void render(const uint32_t* pixels);

        // Cria o menu nativo do Windows (File -> Open)
        void createMenu();

        std::string getSelectedPath() const { return selectedPath; }
        void clearSelectedPath() { selectedPath = ""; }

        bool shouldClose() const { return closed; }

    private:
        void openFileDialog();

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;
        std::string selectedPath = "";
        bool closed = false;
        int width, height;
    };
}