#pragma once

#include <SDL.h>
#include <string>
#include <cstdint>
#include <map>
#include <functional>
#include <memory>
#include "TileViewer.h"
#include "Disassembler.h"

namespace R2NES::Core
{
    class Window
    {
    public:
        Window(const std::string &title, int width, int height, int scale);
        ~Window();

        enum class DisplayMode
        {
            WINDOWED,
            FULLSCREEN_STRETCH,
            FULLSCREEN_ASPECT_8_7
        };

        using KeyCallback = std::function<void(SDL_Keycode, bool)>;
        void setKeyCallback(KeyCallback cb) { keyCallback = cb; }

        using VSyncCallback = std::function<void(bool)>;
        void setVSyncCallback(VSyncCallback cb) { vsyncCallback = cb; }

        using ControllerCallback = std::function<void(int, SDL_GameControllerButton, bool)>;
        void setControllerCallback(ControllerCallback cb) { controllerCallback = cb; }

        // Processa eventos do sistema (como o botão de fechar)
        void pollEvents();

        // Atualiza a textura com os pixels da PPU e desenha na tela
        void render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status, float fps);

        // Atualiza a janela do Disassembler se ela estiver aberta
        void updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);

        // Atualiza a janela do Tile Viewer se ela estiver aberta
        void updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1);

        // Cria o menu nativo do Windows (File -> Open)
        void createMenu();

        std::string getSelectedPath() const { return selectedPath; }

        bool isDisassemblerOpen() const { return disassembler.isOpen(); }

        bool isTileViewerOpen() const { return tileViewer.isOpen(); }

        bool isResetRequested() const { return resetRequested; }

        void clearResetRequest() { resetRequested = false; }

        bool isUnloadRequested() const { return unloadRequested; }

        void clearUnloadRequest() { unloadRequested = false; }

        void clearSelectedPath() { selectedPath = ""; }

        bool isVSyncEnabled() const { return vsyncEnabled; }

        void setVSync(bool enabled);

        void toggleVSync() { setVSync(!vsyncEnabled); }

        void vsyncOff() { setVSync(false); }

        void vsyncOn() { setVSync(true); }

        bool shouldClose() const { return closed; }

        void reset();

        void unload();

        void windowResize(int times);

        void windowBorderlessFullscreenStretch();

        void windowBorderlessFullscreen();

        void setWindowBorderlessFullscreen(DisplayMode currentDisplayMode, Uint32 flags);

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        std::string selectedPath = "";

        TileViewer tileViewer;
        Disassembler disassembler;

        bool closed = false;
        bool resetRequested = false;
        bool unloadRequested = false;
        int width, height, scale;

        KeyCallback keyCallback = nullptr;
        ControllerCallback controllerCallback = nullptr;
        VSyncCallback vsyncCallback = nullptr;

        // Suporte para até 2 controles
        SDL_GameController *controllers[2] = {nullptr, nullptr};

        // Membros para gerenciamento do modo de exibição
        DisplayMode currentDisplayMode = DisplayMode::WINDOWED;

        // Armazena o último estado da janela para restauração
        int lastWindowedX = SDL_WINDOWPOS_CENTERED, lastWindowedY = SDL_WINDOWPOS_CENTERED, lastWindowedW = 0, lastWindowedH = 0;

        bool unlimitedSprites = false;
        bool vsyncEnabled = true;
        bool uncappedSpeed = false;

        void openFileDialog(); // Estes métodos agora são privados e delegam para as classes de ferramentas.
        void openTileViewer(); // O menu chama estes métodos.
        void openDisassembler();
    };
}