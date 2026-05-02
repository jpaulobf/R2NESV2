#pragma once

#include <SDL.h>
#include <string>
#include <cstdint>

namespace R2NES::Core
{
    class Window
    {
    public:
        Window(const std::string &title, int width, int height, int scale);
        ~Window();

        // Processa eventos do sistema (como o botão de fechar)
        void pollEvents();

        // Atualiza a textura com os pixels da PPU e desenha na tela
        void render(const uint32_t *pixels);

        // Atualiza a janela do Tile Viewer se ela estiver aberta
        void updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1);

        // Cria o menu nativo do Windows (File -> Open)
        void createMenu();

        std::string getSelectedPath() const { return selectedPath; }
        
        bool isTileViewerOpen() const { return tileViewerOpen; }

        void clearSelectedPath() { selectedPath = ""; }

        bool shouldClose() const { return closed; }

    private:
        void openFileDialog();
        void openTileViewer();
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        std::string selectedPath = "";

        // Recursos para a janela do Tile Viewer (Pattern Tables)
        SDL_Window *tileWindow = nullptr;
        SDL_Renderer *tileRenderer = nullptr;
        SDL_Texture *tileTexture[2] = {nullptr, nullptr};
        bool tileViewerOpen = false;

        bool closed = false;
        int width, height;
    };
}