#pragma once

#include <SDL.h>
#include <string>
#include <vector>
#include "Common/Common.h"

namespace R2NES::Core
{
    /**
     * @brief Controlador do menu nativo (Win32) usado pela janela principal.
     *
     * Esta classe encapsula a lógica de criação e atualização do menu nativo
     * (barra de menus Win32) a partir de um snapshot de estado (`NativeMenuState`).
     * O objetivo é centralizar os IDs de comando e a construção do menu para
     * manter `Window` mais enxuta e independente da API Win32.
     */
    namespace NativeMenuCommand
    {
        inline constexpr int IDM_FILE_OPEN = 1001;
        inline constexpr int IDM_FILE_EXIT = 1002;
        inline constexpr int IDM_FILE_RESET = 1003;
        inline constexpr int IDM_FILE_UNLOAD = 1004;
        inline constexpr int IDM_DEBUG_TILE_VIEWER = 1006;
        inline constexpr int IDM_DEBUG_DISASSEMBLER = 1007;
        inline constexpr int IDM_DEBUG_RAM_VIEWER = 1008;
        inline constexpr int IDM_DEBUG_PALETTE_VIEWER = 1009;
        inline constexpr int IDM_DEBUG_OAM_VIEWER = 1010;
        inline constexpr int IDM_DEBUG_VRAM_VIEWER = 1011;
        inline constexpr int IDM_FILE_SAVE = 1101;
        inline constexpr int IDM_FILE_LOAD = 1102;
        inline constexpr int IDM_FILE_SAVE_SLOT_1 = 1104;
        inline constexpr int IDM_FILE_SAVE_SLOT_2 = 1105;
        inline constexpr int IDM_FILE_SAVE_SLOT_3 = 1106;
        inline constexpr int IDM_VIEW_VSYNC = 1999;
        inline constexpr int IDM_VIEW_WINDOW_1X = 2000;
        inline constexpr int IDM_VIEW_WINDOW_2X = 2001;
        inline constexpr int IDM_VIEW_WINDOW_3X = 2002;
        inline constexpr int IDM_VIEW_WINDOW_4X = 2003;
        inline constexpr int IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN = 2004;
        inline constexpr int IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH = 2005;
        inline constexpr int IDM_VIEW_SCANLINES = 2006;
        inline constexpr int IDM_VIEW_CROP_OVERSCAN = 2007;
        inline constexpr int IDM_VIEW_FULLCROP_OVERSCAN = 2008;
        inline constexpr int IDM_SOUND_SOUND = 2010;
        inline constexpr int IDM_SOUND_PULSE1 = 2011;
        inline constexpr int IDM_SOUND_PULSE2 = 2012;
        inline constexpr int IDM_SOUND_TRIANGLE = 2013;
        inline constexpr int IDM_SOUND_NOISE = 2014;
        inline constexpr int IDM_SOUND_DMC = 2015;
        inline constexpr int IDM_VIEW_SCANLINES_LEVEL_5 = 2105;
        inline constexpr int IDM_VIEW_SCANLINES_LEVEL_10 = 2110;
        inline constexpr int IDM_VIEW_SCANLINES_LEVEL_15 = 2115;
        inline constexpr int IDM_VIEW_SCANLINES_LEVEL_20 = 2120;
        inline constexpr int IDM_VIEW_SCANLINES_LEVEL_25 = 2125;
        inline constexpr int IDM_VIEW_PALETTE_DEFAULT = 2201;
        inline constexpr int IDM_VIEW_PALETTE_SMOOTH = 2202;
        inline constexpr int IDM_VIEW_PALETTE_NESTOPIA = 2203;
        inline constexpr int IDM_VIEW_PALETTE_WAVEBEAM = 2204;
        inline constexpr int IDM_VIEW_PALETTE_NEON = 2205;
        inline constexpr int IDM_VIEW_SHADERS_NONE = 2301;
        inline constexpr int IDM_VIEW_SHADERS_SCANLINES = 2302;
        inline constexpr int IDM_VIEW_SHADERS_CRT = 2303;
        inline constexpr int IDM_VIEW_SHADERS_CRT3D = 2304;
        inline constexpr int IDM_VIEW_SHADERS_SCALEFX = 2305;
        inline constexpr int IDM_VIEW_SHADERS_XBRZMULTI = 2306;
        inline constexpr int IDM_VIEW_RENDER = 2500;
        inline constexpr int IDM_VIEW_RENDER_TILES = 2501;
        inline constexpr int IDM_VIEW_RENDER_SPRITES = 2502;
        inline constexpr int IDM_HACKS_UNLIMITED_SPRITES = 3000;
        inline constexpr int IDM_HACKS_FAST_FORWARD = 3001;
        inline constexpr int IDM_HACKS_CPU_OVERCLOCK = 3002;
        inline constexpr int IDM_HACKS_REWIND = 3003;
        inline constexpr int IDM_INPUT_INVERT_BAYB = 4000;
        inline constexpr int IDM_INPUT_USE_ZAPPER = 4001;
        inline constexpr int IDM_RECENT_FILE_BASE_ID = 10000;
        inline constexpr int IDI_ICON = 101;
    }

    struct NativeMenuState
    {
        // Indica se um cartucho/ROM está carregado
        bool cartLoaded = false;
        // Lista LRU de ROMs recentes (para montagem do submenu Recent Files)
        std::vector<std::string> recentRoms;
        // Slot de salvamento atualmente selecionado (1-based)
        int saveSlot = 1;
        // Flags que controlam o estado de visibilidade dos viewers de depuração
        bool disassemblerOpen = false;
        bool ramViewerOpen = false;
        bool tileViewerOpen = false;
        bool vramViewerOpen = false;
        bool paletteViewerOpen = false;
        bool oamViewerOpen = false;
        // Configurações visuais e de renderização
        bool vsyncEnabled = false;
        int windowScale = 1;
        bool scanlines = false;
        bool cropOverscan = false;
        bool fullCropOverscan = false;
        bool tilesEnabled = false;
        bool spritesEnabled = false;
        int scanlinesTransparency = 5; // percentual (ex: 5,10,...)
        PaletteType palettePreset = PaletteType::DEFAULT;
        ShaderType shader = ShaderType::NONE;

        // Configurações de áudio
        bool soundEnabled = false;
        bool pulse1Enabled = false;
        bool pulse2Enabled = false;
        bool triangleEnabled = false;
        bool noiseEnabled = false;
        bool dmcEnabled = false;

        // Controles de entrada/hacks
        bool invertBAYB = false; // inverte botões BA/YB
        bool useZapper = false;   // habilita Zapper
        bool unlimitedSprites = false;
        bool fastForwardEnabled = false;
        bool cpuOverclockEnabled = false;
        bool rewindEnabled = false;
    };

    class NativeMenuController
    {
    public:
        /**
         * @brief (Re)cria a barra de menu nativa a partir do `state` fornecido.
         * @param window Janela SDL que receberá o menu (somente Win32 tem suporte).
         * @param state Snapshot com marcadores/flags usados para montar o menu.
         *
         * A função destrói qualquer menu anterior e reconstrói a hierarquia
         * de menus (File/Display/Sound/Debug/...). Em plataformas não-WIN32
         * a chamada é no-op.
         */
        void createMenu(SDL_Window *window, const NativeMenuState &state) const;

        /**
         * @brief Atualiza a marca de seleção (checkbox) de um item já criado.
         * @param window Janela SDL que contém o menu.
         * @param commandId ID do comando (um dos NativeMenuCommand::...).
         * @param checked true para marcar, false para desmarcar.
         */
        void setChecked(SDL_Window *window, int commandId, bool checked) const;

        /**
         * @brief Consulta se um item está marcado no menu nativo.
         * @return true se marcado; false caso contrário ou em plataformas sem suporte.
         */
        bool isChecked(SDL_Window *window, int commandId) const;
    };
}