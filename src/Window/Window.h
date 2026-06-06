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
        using SoundCallback = std::function<void(bool)>;
        using Pulse1Callback = std::function<void(bool)>;
        using Pulse2Callback = std::function<void(bool)>;
        using TriangleCallback = std::function<void(bool)>;
        using NoiseCallback = std::function<void(bool)>;
        using DMCCallback = std::function<void(bool)>;
        using FFCallback = std::function<void(bool)>;
        using UnlimitedSpritesCallback = std::function<void(bool)>;
        using PauseCallback = std::function<void(bool)>;
        using ControllerCallback = std::function<void(int, SDL_GameControllerButton, bool)>;
        using SaveCallback = std::function<void(bool)>;
        using LoadCallback = std::function<void(bool)>;
        using SaveSlotCallback = std::function<void(int)>;

        /* Construtor da classe Window: inicializa a janela SDL, o renderer e as texturas. */
        Window(const std::string &title, int width, int height, int scale);

        /* Destrutor da classe Window: libera os recursos do SDL, ImGui e fecha os controles. */
        ~Window();

        /* Define a função de callback para eventos de teclado. */
        void setKeyCallback(KeyCallback cb) { keyCallback = cb; }

        /* Define a função de callback para mudanças no estado do VSync. */
        void setVSyncCallback(VSyncCallback cb) { vsyncCallback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o som */
        void setSoundCallback(SoundCallback cb) { soundCallback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o canal pulse1 */
        void setPulse1Callback(Pulse1Callback cb) { pulse1Callback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o canal pulse2 */
        void setPulse2Callback(Pulse2Callback cb) { pulse2Callback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o canal triangle */
        void setTriangleCallback(TriangleCallback cb) { triangleCallback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o canal noise */
        void setNoiseCallback(NoiseCallback cb) { noiseCallback = cb; }

        /* Define a função de callback para habilitar ou desabilitar o canal dmc */
        void setDMCCallback(DMCCallback cb) { dmcCallback = cb; }

        /* Define a função de callback para o estado de Fast Forward. */
        void setFFCallback(FFCallback cb) { ffCallback = cb; }

        /* Define a função de callback para o recurso de Unlimited Sprites. */
        void setUnlimitedSpritesCallback(UnlimitedSpritesCallback cb) { unlimitedSpritesCallback = cb; }

        /* Define a função de callback para o estado de Pause. */
        void setPauseCallback(PauseCallback cb) { pauseCallback = cb; }

        /* Define a função de callback para eventos de salvar. */
        void setSaveCallback(SaveCallback cb) { saveCallback = cb; }

        /* Define a função de callback para eventos de carregar. */
        void setLoadCallback(LoadCallback cb) { loadCallback = cb; }

        /* Define a função de callback para eventos de slot de salvamento. */
        void setSaveSlotCallback(SaveSlotCallback cb) { saveSlotCallback = cb; }

        /* Define a função de callback para eventos de botões do controle. */
        void setControllerCallback(ControllerCallback cb) { controllerCallback = cb; }

        // ---------------------------------

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

        /* Verifica se houve um pedido de reset via menu. */
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

        /* Funções para configurar os componentes de som. */
        void setSound(bool enabled);
        void setPulse1(bool enabled);
        void setPulse2(bool enabled);
        void setTriangle(bool enabled);
        void setNoise(bool enabled);
        void setDMC(bool enabled);

        /* Verifica se o som está habilitado. */
        bool isSoundEnabled() const { return soundEnabled; }
        bool isPulse1Enabled() const { return pulse1Enabled; }
        bool isPulse2Enabled() const { return pulse2Enabled; }
        bool isTriangleEnabled() const { return triangleEnabled; }
        bool isNoiseEnabled() const { return noiseEnabled; }
        bool isDMCEnabled() const { return dmcEnabled; }

        /* Ativa o Som */
        void soundOn() { setSound(true); }
        void pulse1On() { setPulse1(true); }
        void pulse2On() { setPulse2(true); }
        void triangleOn() { setTriangle(true); }
        void noiseOn() { setNoise(true); }
        void dmcOn() { setDMC(true); }

        /* Funções para configurar o scanline. */
        void setScanlines(bool enabled);
        void scanlinesOff() { setScanlines(false); }
        void scanlinesOn() { setScanlines(true); }

        /* Desativa o Som */
        void soundOff() { setSound(false); }
        void pulse1Off() { setPulse1(false); }
        void pulse2Off() { setPulse2(false); }
        void triangleOff() { setTriangle(false); }
        void noiseOff() { setNoise(false); }
        void dmcOff() { setDMC(false); }

        /* Verifica se a emulação está pausada. */
        bool isPaused() const { return paused; }

        /* Define o estado de pause da janela e notifica a engine. */
        void setPaused(bool p)
        {
            paused = p;
            if (pauseCallback)
                pauseCallback(paused);
        }

        bool getIsToSave() const { return isToSave; }
        bool getIsToLoad() const { return isToLoad; }
        int getSaveSlot() const { return saveSlot; }
        void setSave(bool save)
        {
            isToSave = save;
            if (saveCallback)
                saveCallback(isToSave);
        }

        void setLoad(bool load)
        {
            isToLoad = load;
            if (loadCallback)
                loadCallback(isToLoad);
        }

        /* Reseta as flags de solicitação de Save/Load. Chamado pela Engine após processar a operação. */
        void resetSaveLoadFlags()
        {
            isToSave = false;
            isToLoad = false;
        }

        void setSaveSlot(int slot)
        {
            saveSlot = slot;
            if (saveSlotCallback)
                saveSlotCallback(saveSlot);
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

        void setCartLoaded(bool loaded)
        {
            cartLoaded = loaded;
            this->createMenu(); // Recria o menu para atualizar o estado do item Unload
        }

    private:
        /* Abre a caixa de diálogo nativa do Windows para abrir arquivos .nes ou .zip. */
        void openFileDialog();

        /* Inicializa e exibe a janela de visualização de tiles (Pattern Tables). */
        void openTileViewer();

        /* Inicializa e exibe a janela do desensamblador de CPU. */
        void openDisassembler();

        /* Inicializa e exibe a janela do RamViewer. */
        void openRamViewer();

        /* Funções para manipular os itens do menu de debug. */
        void windowCheckUncheckMenuItem(int menuItemId, bool isChecked);

        /* Funções para manipular os itens do menu de debug. */
        void toggleMarkMenuItem(int menuItemId, const std::function<void(bool)> &callback);

        /* Funções para manipular os itens do menu de debug. */
        void uncheckAllDebugMenuItems();

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        ImGuiContext *imguiContext = nullptr;
        std::string selectedPath = "";

        MouseState mouseState;
        TileViewer tileViewer;
        RamViewer ramViewer;
        Disassembler disassembler;

        Util::ConfigManager configManager;

        bool closed = false;
        bool resetRequested = false;
        bool unloadRequested = false;
        int width, height, scale;

        std::string title = "R2NESV2 - build 0.7.5 | FPS: %.2f";

        // Teste de Scanlines
        bool scanlines = true;
        int scanlinesTransparency = 5; // 0% a 100% (intensidade do preto)
        std::vector<uint32_t> postProcessBuffer;

        KeyCallback keyCallback = nullptr;
        ControllerCallback controllerCallback = nullptr;
        VSyncCallback vsyncCallback = nullptr;
        UnlimitedSpritesCallback unlimitedSpritesCallback = nullptr;
        PauseCallback pauseCallback = nullptr;
        FFCallback ffCallback = nullptr;
        SoundCallback soundCallback = nullptr;
        Pulse1Callback pulse1Callback = nullptr;
        Pulse2Callback pulse2Callback = nullptr;
        TriangleCallback triangleCallback = nullptr;
        NoiseCallback noiseCallback = nullptr;
        DMCCallback dmcCallback = nullptr;
        SaveCallback saveCallback = nullptr;
        LoadCallback loadCallback = nullptr;
        SaveSlotCallback saveSlotCallback = nullptr;

        // Suporte para até 2 controles
        SDL_GameController *controllers[2] = {nullptr, nullptr};

        // Membros para gerenciamento do modo de exibição
        DisplayMode currentDisplayMode = DisplayMode::WINDOWED;

        // Armazena o último estado da janela para restauração
        int lastWindowedX = SDL_WINDOWPOS_CENTERED, lastWindowedY = SDL_WINDOWPOS_CENTERED, lastWindowedW = 0, lastWindowedH = 0;
        int currentWindowX = 3;
        bool unlimitedSprites = false;
        bool vsyncEnabled = true;
        bool soundEnabled = true;
        bool uncappedSpeed = false;
        bool fastForwardEnabled = true;
        bool paused = false;
        bool tileViewerOpen = false;
        bool disassemblerOpen = false;
        bool ramViewerOpen = false;

        // Configurações de áudio para cada canal
        bool pulse1Enabled = true;
        bool pulse2Enabled = true;
        bool triangleEnabled = true;
        bool noiseEnabled = true;
        bool dmcEnabled = true;

        bool isToSave = false;
        bool isToLoad = false;
        int saveSlot = 1;

        bool cartLoaded = false;
    };
}