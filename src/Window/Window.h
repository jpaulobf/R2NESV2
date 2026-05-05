#pragma once

#include <SDL.h>
#include <string>
#include <cstdint>
#include <map>

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
        void render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly, 
                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);

        // Atualiza a janela do Tile Viewer se ela estiver aberta
        void updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1);

        // Cria o menu nativo do Windows (File -> Open)
        void createMenu();

        std::string getSelectedPath() const { return selectedPath; }

        bool isTileViewerOpen() const { return tileViewerOpen; }

        bool isResetRequested() const { return resetRequested; }
        void clearResetRequest() { resetRequested = false; }

        bool isUnloadRequested() const { return unloadRequested; }
        void clearUnloadRequest() { unloadRequested = false; }

        void clearSelectedPath() { selectedPath = ""; }

        bool shouldClose() const { return closed; }

        void reset();

        void unload();

    private:
        void openFileDialog();
        void openTileViewer();
        void openDisassembler();
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        std::string selectedPath = "";

        // Recursos para a janela do Tile Viewer (Pattern Tables)
        SDL_Window *tileWindow = nullptr;
        SDL_Renderer *tileRenderer = nullptr;
        SDL_Texture *tileTexture[2] = {nullptr, nullptr};
        bool tileViewerOpen = false;

        // Recursos para a janela do Disassembler (Janela nativa + ImGui)
        SDL_Window *disasmWindow = nullptr;
        SDL_Renderer *disasmRenderer = nullptr;
        bool disasmOpen = false;

        bool showDisasm = false;
        bool closed = false;
        bool resetRequested = false;
        bool unloadRequested = false;
        int width, height, scale;
    };
}