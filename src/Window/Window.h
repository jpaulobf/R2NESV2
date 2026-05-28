#pragma once

#include <SDL.h>
#include <string>
#include <cstdint>
#include <map>
#include <functional>
#include <memory>
#include "TileViewer.h"
#include "RamViewer.h"
#include "Disassembler.h"
#include "Util/ConfigManager.h"
#include "Common/Common.h"
#include "imgui.h"

namespace R2NES::Core
{
    class RAM;

    class Window
    {
    public:
        // callbacks
        using KeyCallback = std::function<void(SDL_Keycode, bool)>;
        using VSyncCallback = std::function<void(bool)>;
        using FFCallback = std::function<void(bool)>;
        using UnlimitedSpritesCallback = std::function<void(bool)>;
        using PauseCallback = std::function<void(bool)>;
        using ControllerCallback = std::function<void(int, SDL_GameControllerButton, bool)>;

        /* Construtor da classe Window: inicializa a janela SDL, o renderer e as texturas. */
        Window(const std::string &title, int width, int height, int scale);

        /* Destrutor da classe Window: libera os recursos do SDL, ImGui e fecha os controles. */
        ~Window();

        /* Define a função de callback para eventos de teclado. */
        void setKeyCallback(KeyCallback cb) { keyCallback = cb; }

        /* Define a função de callback para mudanças no estado do VSync. */
        void setVSyncCallback(VSyncCallback cb) { vsyncCallback = cb; }

        /* Define a função de callback para o estado de Fast Forward. */
        void setFFCallback(FFCallback cb) { ffCallback = cb; }

        /* Define a função de callback para o recurso de Unlimited Sprites. */
        void setUnlimitedSpritesCallback(UnlimitedSpritesCallback cb) { unlimitedSpritesCallback = cb; }

        /* Define a função de callback para o estado de Pause. */
        void setPauseCallback(PauseCallback cb) { pauseCallback = cb; }

        /* Define a função de callback para eventos de botões do controle. */
        void setControllerCallback(ControllerCallback cb) { controllerCallback = cb; }

        /* Processa eventos do sistema (como o botão de fechar, entradas de teclado e mouse). */
        void pollEvents();

        /* Atualiza a textura principal com os pixels da PPU e renderiza na tela. */
        void render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status, float fps);

        /* Atualiza os dados e renderiza a janela do Disassembler se estiver aberta. */
        void updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);

        /* Atualiza os pixels das Pattern Tables na janela do Tile Viewer. */
        void updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1);

        /* Atualiza os dados e renderiza a janela do RamViewer se estiver aberta. */
        void updateRamViewer(RAM *ram);

        /* Cria e configura o menu nativo do Windows (File, Display, Debug, etc). */
        void createMenu();

        /* Retorna o caminho do arquivo ROM selecionado no diálogo de abertura. */
        std::string getSelectedPath() const { return selectedPath; }

        /* Retorna o estado atual das coordenadas e botões do mouse. */
        MouseState getMouseState() const { return mouseState; }

        /* Verifica se a janela do Disassembler está aberta. */
        bool isDisassemblerOpen() const { return disassembler.isOpen(); }

        /* Verifica se a janela do Tile Viewer está aberta. */
        bool isTileViewerOpen() const { return tileViewer.isOpen(); }

        /* Verifica se houve um pedido de reset via menu. */
        /* Verifica se a janela do RamViewer está aberta. */
        bool isRamViewerOpen() const { return ramViewer.isOpen(); }

        bool isResetRequested() const { return resetRequested; }

        /* Limpa a flag de solicitação de reset. */
        void clearResetRequest() { resetRequested = false; }

        /* Verifica se houve um pedido de descarregamento (unload) de ROM via menu. */
        bool isUnloadRequested() const { return unloadRequested; }

        /* Limpa a flag de solicitação de descarregamento de ROM. */
        void clearUnloadRequest() { unloadRequested = false; }

        /* Limpa o caminho da ROM selecionada. */
        void clearSelectedPath() { selectedPath = ""; }

        /* Verifica se o VSync está habilitado. */
        bool isVSyncEnabled() const { return vsyncEnabled; }

        /* Habilita ou desabilita o VSync, recriando o renderer para aplicar a mudança. */
        void setVSync(bool enabled);

        /* Alterna o estado do VSync. */
        void toggleVSync() { setVSync(!vsyncEnabled); }

        /* Desabilita o VSync. */
        void vsyncOff() { setVSync(false); }

        /* Habilita o VSync. */
        void vsyncOn() { setVSync(true); }

        /* Verifica se o hack de Unlimited Sprites está habilitado. */
        bool isUnlimitedSpritesEnabled() const { return unlimitedSprites; }

        /* Ativa ou desativa o suporte a sprites ilimitados por scanline. */
        void setUnlimitedSprites(bool enabled);

        /* Desabilita o recurso de Unlimited Sprites. */
        void unlimitedSpritesOff() { setUnlimitedSprites(false); }

        /* Habilita o recurso de Unlimited Sprites. */
        void unlimitedSpritesOn() { setUnlimitedSprites(true); }

        /* Verifica se o Fast Forward está habilitado nas configurações. */
        bool isFastForwardEnabled() const { return fastForwardEnabled; }

        /* Define o estado do Fast Forward e notifica a engine. */
        void setFastForward(bool enabled);

        /* Desativa o Fast Forward. */
        void fastForwardOff() { setFastForward(false); }

        /* Ativa o Fast Forward. */
        void fastForwardOn() { setFastForward(true); }

        /* Verifica se a emulação está pausada. */
        bool isPaused() const { return paused; }

        /* Define o estado de pause da janela e notifica a engine. */
        void setPaused(bool p)
        {
            paused = p;
            if (pauseCallback)
                pauseCallback(paused);
        }

        /* Verifica se a janela principal foi fechada. */
        bool shouldClose() const { return closed; }

        /* Inicia a lógica de reset de janelas de debug e sinaliza a engine. */
        void reset();

        /* Inicia a lógica de fechamento de ferramentas e sinaliza o unload da ROM. */
        void unload();

        /* Redimensiona a janela principal multiplicando o tamanho base pelo fator informado. */
        void windowResize(int times);

        /* Configura a janela para o modo Fullscreen sem bordas esticando a imagem. */
        void windowBorderlessFullscreenStretch();

        /* Configura a janela para o modo Fullscreen sem bordas mantendo o aspecto 8:7. */
        void windowBorderlessFullscreen();

        /* Função genérica para configurar o modo de tela cheia sem bordas. */
        void setWindowBorderlessFullscreen(DisplayMode currentDisplayMode, Uint32 flags);

    private:
        /* Abre a caixa de diálogo nativa do Windows para abrir arquivos .nes ou .zip. */
        void openFileDialog();

        /* Inicializa e exibe a janela de visualização de tiles (Pattern Tables). */
        void openTileViewer();

        /* Inicializa e exibe a janela do desensamblador de CPU. */
        void openDisassembler();

        /* Inicializa e exibe a janela do RamViewer. */
        void openRamViewer();

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        ImGuiContext* imguiContext = nullptr;
        MouseState mouseState;
        std::string selectedPath = "";

        TileViewer tileViewer;
        RamViewer ramViewer;
        Disassembler disassembler;

        Util::ConfigManager configManager;

        bool closed = false;
        bool resetRequested = false;
        bool unloadRequested = false;
        int width, height, scale;

        std::string title = "R2NESV2 - build 0.6.0 | FPS: %.2f";

        KeyCallback keyCallback = nullptr;
        ControllerCallback controllerCallback = nullptr;
        VSyncCallback vsyncCallback = nullptr;
        UnlimitedSpritesCallback unlimitedSpritesCallback = nullptr;
        PauseCallback pauseCallback = nullptr;
        FFCallback ffCallback = nullptr;

        // Suporte para até 2 controles
        SDL_GameController *controllers[2] = {nullptr, nullptr};

        // Membros para gerenciamento do modo de exibição
        DisplayMode currentDisplayMode = DisplayMode::WINDOWED;

        // Armazena o último estado da janela para restauração
        int lastWindowedX = SDL_WINDOWPOS_CENTERED, lastWindowedY = SDL_WINDOWPOS_CENTERED, lastWindowedW = 0, lastWindowedH = 0;
        int currentWindowX = 3;
        bool unlimitedSprites = false;
        bool vsyncEnabled = true;
        bool uncappedSpeed = false;
        bool fastForwardEnabled = true;
        bool paused = false;
    };
}