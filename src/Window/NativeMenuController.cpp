#include "NativeMenuController.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <SDL_syswm.h>
#endif

namespace R2NES::Core
{
#ifdef _WIN32
    namespace
    {
        constexpr int IconResourceId = 101;
        // Helper para adicionar um item ao menu Win32 com label wide-char.
        // O parâmetro `checked` determina se o item aparece com marcação.
        // `enabled` controla se o item está habilitado ou esmaecido.
        void appendItem(HMENU menu, int commandId, const wchar_t *label, bool checked = false, bool enabled = true)
        {
            UINT flags = MF_STRING | (checked ? MF_CHECKED : MF_UNCHECKED) | (enabled ? MF_ENABLED : MF_DISABLED);
            AppendMenuW(menu, flags, commandId, label);
        }
    }
#endif

    void NativeMenuController::createMenu(SDL_Window *window, const NativeMenuState &state) const
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(window, &wmInfo))
            return;

        HWND hwnd = wmInfo.info.win.window;
        if (HMENU oldMenu = GetMenu(hwnd))
            DestroyMenu(oldMenu);

        if (HICON icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IconResourceId)))
        {
            SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        }

        // Cria a barra e os submenus usados pela aplicação
        HMENU menuBar = CreateMenu();
        HMENU fileMenu = CreateMenu();
        HMENU inputMenu = CreateMenu();
        HMENU debugMenu = CreateMenu();
        HMENU displayMenu = CreateMenu();
        HMENU soundMenu = CreateMenu();
        HMENU hacksMenu = CreateMenu();

        // Seções do menu 'File'. Habilitamos/desabilitamos entradas conforme `state`.
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_OPEN, L"&Open ROM...");
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_RESET, L"&Reset");
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_UNLOAD, L"&Unload", false, state.cartLoaded);
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_SAVE, L"&Save State\tF5", false, state.cartLoaded);
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_LOAD, L"&Load State\tF6", false, state.cartLoaded);

        HMENU saveSlotMenu = CreatePopupMenu();
        appendItem(saveSlotMenu, NativeMenuCommand::IDM_FILE_SAVE_SLOT_1, L"Slot 1", state.saveSlot == 1);
        appendItem(saveSlotMenu, NativeMenuCommand::IDM_FILE_SAVE_SLOT_2, L"Slot 2", state.saveSlot == 2);
        appendItem(saveSlotMenu, NativeMenuCommand::IDM_FILE_SAVE_SLOT_3, L"Slot 3", state.saveSlot == 3);
        AppendMenuW(fileMenu, MF_POPUP | (state.cartLoaded ? MF_ENABLED : MF_DISABLED), reinterpret_cast<UINT_PTR>(saveSlotMenu), L"&Save State Slot");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);

        // Submenu 'Recent Files' (usa AppendMenuA para labels em ASCII)
        HMENU recentFilesMenu = CreatePopupMenu();
        if (state.recentRoms.empty())
        {
            AppendMenuW(recentFilesMenu, MF_STRING | MF_GRAYED, 0, L"No Recent Files");
        }
        else
        {
            for (size_t index = 0; index < state.recentRoms.size() && index < 10; ++index)
            {
                const std::string &path = state.recentRoms[index];
                // Extrai somente o nome do arquivo para exibir no menu
                std::string fileName = path.substr(path.find_last_of("\\/") + 1);
                AppendMenuA(recentFilesMenu, MF_STRING, NativeMenuCommand::IDM_RECENT_FILE_BASE_ID + static_cast<int>(index), fileName.c_str());
            }
        }
        AppendMenuW(fileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(recentFilesMenu), L"&Recent Files");
        AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(fileMenu, NativeMenuCommand::IDM_FILE_EXIT, L"&Exit");

        // Submenu 'Debug' -> contém toggles para os viewers de depuração
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_DISASSEMBLER, L"&Disassembler", state.disassemblerOpen);
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_RAM_VIEWER, L"&RAM Viewer", state.ramViewerOpen);
        AppendMenuW(debugMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_TILE_VIEWER, L"&Tile Viewer", state.tileViewerOpen);
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_VRAM_VIEWER, L"&VRAM Viewer", state.vramViewerOpen);
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_PALETTE_VIEWER, L"&Palette Viewer", state.paletteViewerOpen);
        appendItem(debugMenu, NativeMenuCommand::IDM_DEBUG_OAM_VIEWER, L"&OAM Viewer", state.oamViewerOpen);

        // Submenu 'Display' -> opções de render e visualização
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_VSYNC, L"&VSync", state.vsyncEnabled);
        AppendMenuW(displayMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_1X, L"&1x", state.windowScale == 1);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_2X, L"&2x", state.windowScale == 2);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_3X, L"&3x", state.windowScale == 3);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_4X, L"&4x", state.windowScale == 4);
        AppendMenuW(displayMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH, L"&Borderless Fullscreen Stretch");
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN, L"&Borderless Fullscreen");
        AppendMenuW(displayMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_SCANLINES, L"&Scanlines", state.scanlines);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_CROP_OVERSCAN, L"&Top Crop Overscan (8px)", state.cropOverscan);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_FULLCROP_OVERSCAN, L"&Full Crop Overscan (8px, 8px, 8px, 8px)", state.fullCropOverscan);
        AppendMenuW(displayMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_RENDER_TILES, L"&Render Tiles", state.tilesEnabled);
        appendItem(displayMenu, NativeMenuCommand::IDM_VIEW_RENDER_SPRITES, L"&Render Sprites", state.spritesEnabled);

        // Submenus auxiliares (Scanlines, Palettes, Shaders)
        HMENU scanlineMenu = CreatePopupMenu();
        appendItem(scanlineMenu, NativeMenuCommand::IDM_VIEW_SCANLINES_LEVEL_5, L"5%", state.scanlinesTransparency == 5);
        appendItem(scanlineMenu, NativeMenuCommand::IDM_VIEW_SCANLINES_LEVEL_10, L"10%", state.scanlinesTransparency == 10);
        appendItem(scanlineMenu, NativeMenuCommand::IDM_VIEW_SCANLINES_LEVEL_15, L"15%", state.scanlinesTransparency == 15);
        appendItem(scanlineMenu, NativeMenuCommand::IDM_VIEW_SCANLINES_LEVEL_20, L"20%", state.scanlinesTransparency == 20);
        appendItem(scanlineMenu, NativeMenuCommand::IDM_VIEW_SCANLINES_LEVEL_25, L"25%", state.scanlinesTransparency == 25);
        AppendMenuW(displayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(scanlineMenu), L"&Scanlines Level");

        HMENU paletteMenu = CreatePopupMenu();
        appendItem(paletteMenu, NativeMenuCommand::IDM_VIEW_PALETTE_DEFAULT, L"Default", state.palettePreset == PaletteType::DEFAULT);
        appendItem(paletteMenu, NativeMenuCommand::IDM_VIEW_PALETTE_SMOOTH, L"Smooth", state.palettePreset == PaletteType::SMOOTH);
        appendItem(paletteMenu, NativeMenuCommand::IDM_VIEW_PALETTE_NESTOPIA, L"Nestopia Emulator", state.palettePreset == PaletteType::NESTOPIA);
        appendItem(paletteMenu, NativeMenuCommand::IDM_VIEW_PALETTE_WAVEBEAM, L"WaveBeam", state.palettePreset == PaletteType::WAVEBEAM);
        appendItem(paletteMenu, NativeMenuCommand::IDM_VIEW_PALETTE_NEON, L"Neon", state.palettePreset == PaletteType::NEON);
        AppendMenuW(displayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(paletteMenu), L"&Palettes Preset");

        HMENU shaderMenu = CreatePopupMenu();
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_NONE, L"None", state.shader == ShaderType::NONE);
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_SCANLINES, L"Scanlines", state.shader == ShaderType::SCANLINES);
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_CRT, L"CRT", state.shader == ShaderType::CRT);
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_CRT3D, L"CRT 3D", state.shader == ShaderType::CRT3D);
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_SCALEFX, L"ScaleFX", state.shader == ShaderType::SCALEFX);
        appendItem(shaderMenu, NativeMenuCommand::IDM_VIEW_SHADERS_XBRZMULTI, L"xBRZ Multi", state.shader == ShaderType::XBRZMULTI);
        AppendMenuW(displayMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(shaderMenu), L"&Shaders");

        // Submenu 'Sound' -> toggles por canal
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_SOUND, L"&Master Sound", state.soundEnabled);
        AppendMenuW(soundMenu, MF_SEPARATOR, 0, nullptr);
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_PULSE1, L"&Pulse 1", state.pulse1Enabled);
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_PULSE2, L"&Pulse 2", state.pulse2Enabled);
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_TRIANGLE, L"&Triangle", state.triangleEnabled);
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_NOISE, L"&Noise", state.noiseEnabled);
        appendItem(soundMenu, NativeMenuCommand::IDM_SOUND_DMC, L"&DMC", state.dmcEnabled);

        // Submenu 'Input' -> opções de entrada
        appendItem(inputMenu, NativeMenuCommand::IDM_INPUT_INVERT_BAYB, L"&Invert BA/YB Buttons", state.invertBAYB);
        appendItem(inputMenu, NativeMenuCommand::IDM_INPUT_USE_ZAPPER, L"&Enable Zapper", state.useZapper);

        // Submenu 'Hacks' -> recursos experimentais/cheats
        appendItem(hacksMenu, NativeMenuCommand::IDM_HACKS_UNLIMITED_SPRITES, L"&Enable Unlimited Sprites", state.unlimitedSprites);
        appendItem(hacksMenu, NativeMenuCommand::IDM_HACKS_FAST_FORWARD, L"&Enable Fast Forward", state.fastForwardEnabled);
        appendItem(hacksMenu, NativeMenuCommand::IDM_HACKS_CPU_OVERCLOCK, L"&Enable CPU Overclock", state.cpuOverclockEnabled);
        appendItem(hacksMenu, NativeMenuCommand::IDM_HACKS_REWIND, L"&Enable Rewind", state.rewindEnabled);

        // Rewind Level popup: disabled when rewind is disabled
        HMENU rewindLevelMenu = CreatePopupMenu();
        appendItem(rewindLevelMenu, NativeMenuCommand::IDM_HACKS_REWIND_LEVEL_LIGHT, L"Light", state.rewindPrecisionLevel == 0);
        appendItem(rewindLevelMenu, NativeMenuCommand::IDM_HACKS_REWIND_LEVEL_NORMAL, L"Normal", state.rewindPrecisionLevel == 1);
        appendItem(rewindLevelMenu, NativeMenuCommand::IDM_HACKS_REWIND_LEVEL_PRECISE, L"Precise", state.rewindPrecisionLevel == 2);
        AppendMenuW(hacksMenu, MF_POPUP | (state.rewindEnabled ? MF_ENABLED : MF_DISABLED), reinterpret_cast<UINT_PTR>(rewindLevelMenu), L"&Rewind Level");

        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(displayMenu), L"&Display");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(soundMenu), L"&Sound");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(inputMenu), L"&Input");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(debugMenu), L"&Debug");
        AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hacksMenu), L"&Hacks");
        // Instala a nova barra de menu na janela
        SetMenu(hwnd, menuBar);
#else
        (void)window;
        (void)state;
#endif
    }

    void NativeMenuController::setChecked(SDL_Window *window, int commandId, bool checked) const
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            if (HMENU menu = GetMenu(wmInfo.info.win.window))
                CheckMenuItem(menu, commandId, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
        }
#else
        (void)window;
        (void)commandId;
        (void)checked;
#endif
    }

    bool NativeMenuController::isChecked(SDL_Window *window, int commandId) const
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            if (HMENU menu = GetMenu(wmInfo.info.win.window))
                return (GetMenuState(menu, commandId, MF_BYCOMMAND) & MF_CHECKED) != 0;
        }
#else
        (void)window;
        (void)commandId;
#endif
        return false;
    }
}