#include "Window.h"
#include <iostream>
#include <SDL_syswm.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include <filesystem>
#include "imgui_impl_sdlrenderer2.h"
#include "Util/ConfigManager.h"
#include "Common/Common.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#define IDM_FILE_OPEN 1001
#define IDM_FILE_EXIT 1002
#define IDM_FILE_RESET 1003
#define IDM_FILE_UNLOAD 1004
#define IDM_FILE_SAVE 1101
#define IDM_FILE_LOAD 1102
#define IDM_FILE_SAVE_SLOT 1103
#define IDM_FILE_SAVE_SLOT_1 1104
#define IDM_FILE_SAVE_SLOT_2 1105
#define IDM_FILE_SAVE_SLOT_3 1106
#define IDM_RECENT_FILE_BASE_ID 10000
#define IDM_DEBUG_TILE_VIEWER 1006
#define IDM_DEBUG_DISASSEMBLER 1007
#define IDM_DEBUG_PALETTE_VIEWER 1009
#define IDM_DEBUG_OAM_VIEWER 1010
#define IDM_DEBUG_VRAM_VIEWER 1011
#define IDM_DEBUG_RAM_VIEWER 1008
#define IDM_VIEW_VSYNC 1999
#define IDM_VIEW_WINDOW_1X 2000
#define IDM_VIEW_WINDOW_2X 2001
#define IDM_VIEW_WINDOW_3X 2002
#define IDM_VIEW_WINDOW_4X 2003
#define IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN 2004
#define IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH 2005
#define IDM_VIEW_SCANLINES 2006
#define IDM_VIEW_CROP_OVERSCAN 2007
#define IDM_VIEW_FULLCROP_OVERSCAN 2008
#define IDM_VIEW_SCANLINES_LEVEL_5 2105
#define IDM_VIEW_SCANLINES_LEVEL_10 2110
#define IDM_VIEW_SCANLINES_LEVEL_15 2115
#define IDM_VIEW_SCANLINES_LEVEL_20 2120
#define IDM_VIEW_SCANLINES_LEVEL_25 2125
#define IDM_VIEW_PALETTES 2200
#define IDM_VIEW_PALETTE_DEFAULT 2201
#define IDM_VIEW_PALETTE_SMOOTH 2202
#define IDM_VIEW_PALETTE_NESTOPIA 2203
#define IDM_VIEW_PALETTE_WAVEBEAM 2204
#define IDM_VIEW_PALETTE_NEON 2205
#define IDM_SOUND_SOUND 2010
#define IDM_SOUND_PULSE1 2011
#define IDM_SOUND_PULSE2 2012
#define IDM_SOUND_TRIANGLE 2013
#define IDM_SOUND_NOISE 2014
#define IDM_SOUND_DMC 2015
#define IDM_VIEW_SHADERS 2300
#define IDM_VIEW_SHADERS_NONE 2301
#define IDM_VIEW_SHADERS_SCANLINES 2302
#define IDM_VIEW_SHADERS_CRT 2303
#define IDM_VIEW_SHADERS_CRT3D 2304
#define IDM_VIEW_SHADERS_SCALEFX 2305
#define IDM_VIEW_SHADERS_XBRZMULTI 2306
#define IDM_HACKS_UNLIMITED_SPRITES 3000
#define IDM_HACKS_FAST_FORWARD 3001
#define IDI_ICON 101
#define IDM_INPUT_INVERT_BAYB 4000
#define IDM_INPUT_USE_ZAPPER 4001

namespace R2NES::Core
{
    Window::Window(const std::string &title, int w, int h, int s)
        : width(w), height(h), scale(s)
    {
        // Store initial windowed state
        lastWindowedW = w * s;
        lastWindowedH = h * s;

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
        {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }
        window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w * scale, h * scale,
            SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }

        if (vsyncEnabled)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        }
        else
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        }

        // Garante que o fundo da janela principal também comece preto
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);

        // Criamos a textura no formato ARGB8888 para bater com o frameBuffer da PPU
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            w, h);

        // Habilita o SDL para capturar mensagens nativas do Windows (necessário para o Menu)
        SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        imguiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // Redimensiona a janela para 3x por padrão.
        this->windowResize(currentWindowX);

        // Informa a Engine o estado inicial do VSync
        if (vsyncCallback)
            this->vsyncCallback(vsyncEnabled);
    }

    Window::~Window()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (controllers[i])
            {
                SDL_GameControllerClose(controllers[i]);
                controllers[i] = nullptr;
            }
        }

        if (imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext(imguiContext);
        }

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void Window::windowCheckUncheckMenuItem(int menuItemId, bool isChecked)
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            HMENU hMenu = GetMenu(wmInfo.info.win.window);
            if (hMenu)
                CheckMenuItem(hMenu, menuItemId, MF_BYCOMMAND | (isChecked ? MF_CHECKED : MF_UNCHECKED));
        }
#endif
    }

    void Window::toggleMarkMenuItem(int menuItemId, const std::function<void(bool)> &callback)
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            HMENU hMenu = GetMenu(wmInfo.info.win.window);
            UINT state = GetMenuState(hMenu, menuItemId, MF_BYCOMMAND);
            CheckMenuItem(hMenu, menuItemId, MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
            callback(state);
        }
#endif
    }

    void Window::pollEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            // Processa eventos para o ImGui da janela principal
            if (imguiContext)
            {
                ImGui::SetCurrentContext(imguiContext);
                ImGui_ImplSDL2_ProcessEvent(&e);
            }

            // Só processa eventos do ImGui se a janela de debug estiver ativa
            tileViewer.handleEvent(&e);
            disassembler.handleEvent(&e);
            paletteViewer.handleEvent(&e);
            oamViewer.handleEvent(&e);
            vramViewer.handleEvent(&e);
            ramViewer.handleEvent(&e);

            if (e.type == SDL_QUIT)
                closed = true;

            // Sai do modo tela cheia e restaura o menu ao apertar ESC
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            {
                if (currentDisplayMode != DisplayMode::WINDOWED)
                {
                    SDL_SetWindowFullscreen(window, 0);
                    SDL_SetWindowBordered(window, SDL_TRUE);
                    SDL_SetWindowPosition(window, lastWindowedX, lastWindowedY);
                    SDL_SetWindowSize(window, lastWindowedW, lastWindowedH);
                    createMenu();
                    currentDisplayMode = DisplayMode::WINDOWED;
                }
            }

            // Gerenciamento de Conexão de Controles
            if (e.type == SDL_CONTROLLERDEVICEADDED)
            {
                int deviceIndex = e.cdevice.which;
                if (SDL_IsGameController(deviceIndex))
                {
                    for (int i = 0; i < 2; ++i)
                    {
                        if (!controllers[i])
                        {
                            controllers[i] = SDL_GameControllerOpen(deviceIndex);
                            std::cout << "Controller connected as Player " << (i + 1) << ": "
                                      << SDL_GameControllerName(controllers[i]) << std::endl;
                            break;
                        }
                    }
                }
            }
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED)
            {
                SDL_GameController *removed = SDL_GameControllerFromInstanceID(e.cdevice.which);
                for (int i = 0; i < 2; ++i)
                {
                    if (controllers[i] == removed)
                    {
                        std::cout << "Controller disconnected from Player " << (i + 1) << std::endl;
                        SDL_GameControllerClose(removed);
                        controllers[i] = nullptr;
                        break;
                    }
                }
            }

            // Eventos de Botões do Controle
            else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
            {
                if (controllerCallback)
                {
                    int playerNum = (SDL_GameControllerFromInstanceID(e.cbutton.which) == controllers[0]) ? 1 : 2;
                    controllerCallback(playerNum, (SDL_GameControllerButton)e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN);
                }
            }

            // Captura de Mouse para Zapper
            if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
            {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                mouseState.leftButton = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT));

                int winW, winH;
                SDL_GetWindowSize(window, &winW, &winH);

                if (currentDisplayMode == DisplayMode::FULLSCREEN_ASPECT_8_7)
                {
                    double target_aspect = 8.0 / 7.0;
                    int renderW, renderH, offsetX, offsetY;
                    if ((double)winW / winH > target_aspect)
                    {
                        renderH = winH;
                        renderW = (int)(winH * target_aspect);
                    }
                    else
                    {
                        renderW = winW;
                        renderH = (int)(winW / target_aspect);
                    }
                    offsetX = (winW - renderW) / 2;
                    offsetY = (winH - renderH) / 2;

                    mouseState.x = (int)((float)(mouseX - offsetX) * 256.0f / (float)renderW);
                    mouseState.y = (int)((float)(mouseY - offsetY) * 240.0f / (float)renderH);
                }
                else
                {
                    mouseState.x = (int)((float)mouseX * 256.0f / (float)winW);
                    mouseState.y = (int)((float)mouseY * 240.0f / (float)winH);
                }

                // Garante que os valores não saiam do range do NES (Clamping)
                mouseState.x = std::max(0, std::min(255, mouseState.x));
                mouseState.y = std::max(0, std::min(239, mouseState.y));
            }

            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.keysym.sym != SDLK_ESCAPE)
            {
                if (keyCallback)
                {
                    bool isPressed = (e.type == SDL_KEYDOWN);
                    keyCallback(e.key.keysym.sym, isPressed);
                }
            }

            // Trata mensagens do Menu do Windows
            if (e.type == SDL_SYSWMEVENT)
            {
#ifdef _WIN32
                if (e.syswm.msg->msg.win.msg == WM_COMMAND)
                {
                    if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_OPEN)
                    {
                        openFileDialog();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_RESET)
                    {
                        reset();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_EXIT)
                    {
                        closed = true;
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_UNLOAD)
                    {
                        unload();
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_SAVE)
                    {
                        this->setLoad(false);
                        this->setSave(true);
                        std::cout << "Saving State - Slot " << this->getSaveSlot() << "..." << std::endl;
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_LOAD)
                    {
                        this->setSave(false);
                        this->setLoad(true);
                        std::cout << "Loading State - Slot " << this->getSaveSlot() << "..." << std::endl;
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) >= IDM_FILE_SAVE_SLOT_1 &&
                             LOWORD(e.syswm.msg->msg.win.wParam) <= IDM_FILE_SAVE_SLOT_3)
                    {
                        int id = LOWORD(e.syswm.msg->msg.win.wParam);
                        this->setSaveSlot(id - IDM_FILE_SAVE_SLOT_1 + 1);
                        createMenu(); // Atualiza os radio buttons de slot no menu
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) >= IDM_RECENT_FILE_BASE_ID && LOWORD(e.syswm.msg->msg.win.wParam) < IDM_RECENT_FILE_BASE_ID + 10)
                    {
                        int index = LOWORD(e.syswm.msg->msg.win.wParam) - IDM_RECENT_FILE_BASE_ID;
                        const auto &recentRoms = configManager.getRecentRoms();
                        if (index < recentRoms.size())
                        {
                            auto it = recentRoms.begin();
                            std::advance(it, index);
                            selectedPath = *it;
                            configManager.addRomToList(selectedPath);
                            createMenu();
                        }
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_TILE_VIEWER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_TILE_VIEWER, true);
                        openTileViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_PALETTE_VIEWER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_PALETTE_VIEWER, true);
                        openPaletteViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_DISASSEMBLER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_DISASSEMBLER, true);
                        openDisassembler();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_RAM_VIEWER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_RAM_VIEWER, true);
                        openRamViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_OAM_VIEWER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_OAM_VIEWER, true);
                        openOamViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_VRAM_VIEWER)
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_VRAM_VIEWER, true);
                        openVramViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_VSYNC)
                    {
                        toggleMarkMenuItem(IDM_VIEW_VSYNC, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->vsyncOff();
                            else
                                this->vsyncOn(); });
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_1X)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_1X, MF_BYCOMMAND | MF_CHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_2X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_3X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_4X, MF_BYCOMMAND | MF_UNCHECKED);
                        windowResize(1);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_2X)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_2X, MF_BYCOMMAND | MF_CHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_1X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_3X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_4X, MF_BYCOMMAND | MF_UNCHECKED);
                        windowResize(2);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_3X)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_3X, MF_BYCOMMAND | MF_CHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_1X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_2X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_4X, MF_BYCOMMAND | MF_UNCHECKED);
                        windowResize(3);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_4X)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_4X, MF_BYCOMMAND | MF_CHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_1X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_2X, MF_BYCOMMAND | MF_UNCHECKED);
                        CheckMenuItem(hMenu, IDM_VIEW_WINDOW_3X, MF_BYCOMMAND | MF_UNCHECKED);
                        windowResize(4);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH)
                    {
                        windowBorderlessFullscreenStretch();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN)
                    {
                        windowBorderlessFullscreen();
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_SCANLINES)
                    {
                        toggleMarkMenuItem(IDM_VIEW_SCANLINES, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->scanlinesOff();
                            else
                                this->scanlinesOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_CROP_OVERSCAN)
                    {
                        toggleMarkMenuItem(IDM_VIEW_CROP_OVERSCAN, [this](bool currentlyChecked)
                                           {
                            // Inverte o estado atual (atribuindo a uma variável membro)
                            this->cropOverscan = !this->cropOverscan; // Usa o estado atual da variável

                            // Se ativou o crop normal, desativa o full crop
                            if (this->cropOverscan) {
                                this->fullCropOverscan = false;
                                windowCheckUncheckMenuItem(IDM_VIEW_FULLCROP_OVERSCAN, false);
                            }

                            // Se estiver em modo janela, redimensiona para ajustar ao novo conteúdo
                            if (this->currentDisplayMode == DisplayMode::WINDOWED) {
                                this->windowResize(this->currentWindowX);
                            }

                            std::cout << "Window: Crop Overscan " << (this->cropOverscan ? "On" : "Off") << std::endl; });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_FULLCROP_OVERSCAN)
                    {
                        toggleMarkMenuItem(IDM_VIEW_FULLCROP_OVERSCAN, [this](bool currentlyChecked)
                                           {
                            // Inverte o estado atual (atribuindo a uma variável membro)
                            this->fullCropOverscan = !this->fullCropOverscan; // Usa o estado atual da variável

                            // Se ativou o full crop, desativa o crop normal
                            if (this->fullCropOverscan) {
                                this->cropOverscan = false;
                                windowCheckUncheckMenuItem(IDM_VIEW_CROP_OVERSCAN, false);
                            }

                            // Se estiver em modo janela, redimensiona para ajustar ao novo conteúdo
                            if (this->currentDisplayMode == DisplayMode::WINDOWED) {
                                this->windowResize(this->currentWindowX);
                            }

                            std::cout << "Window: Full Crop Overscan " << (this->fullCropOverscan ? "On" : "Off") << std::endl; });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) >= IDM_VIEW_SCANLINES_LEVEL_5 &&
                             LOWORD(e.syswm.msg->msg.win.wParam) <= IDM_VIEW_SCANLINES_LEVEL_25)
                    {
                        int id = LOWORD(e.syswm.msg->msg.win.wParam);
                        if (id == IDM_VIEW_SCANLINES_LEVEL_5)
                            scanlinesTransparency = 5;
                        if (id == IDM_VIEW_SCANLINES_LEVEL_10)
                            scanlinesTransparency = 10;
                        if (id == IDM_VIEW_SCANLINES_LEVEL_15)
                            scanlinesTransparency = 15;
                        if (id == IDM_VIEW_SCANLINES_LEVEL_20)
                            scanlinesTransparency = 20;
                        if (id == IDM_VIEW_SCANLINES_LEVEL_25)
                            scanlinesTransparency = 25;
                        createMenu(); // Atualiza os checks no menu
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) >= IDM_VIEW_PALETTE_DEFAULT &&
                             LOWORD(e.syswm.msg->msg.win.wParam) <= IDM_VIEW_PALETTE_NEON)
                    {
                        int id = LOWORD(e.syswm.msg->msg.win.wParam);
                        if (id == IDM_VIEW_PALETTE_DEFAULT)
                            setPalettePreset(PaletteType::DEFAULT);
                        if (id == IDM_VIEW_PALETTE_SMOOTH)
                            setPalettePreset(PaletteType::SMOOTH);
                        if (id == IDM_VIEW_PALETTE_NESTOPIA)
                            setPalettePreset(PaletteType::NESTOPIA);
                        if (id == IDM_VIEW_PALETTE_WAVEBEAM)
                            setPalettePreset(PaletteType::WAVEBEAM);
                        if (id == IDM_VIEW_PALETTE_NEON)
                            setPalettePreset(PaletteType::NEON);
                        createMenu();
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) >= IDM_VIEW_SHADERS_NONE &&
                             LOWORD(e.syswm.msg->msg.win.wParam) <= IDM_VIEW_SHADERS_XBRZMULTI)
                    {
                        int id = LOWORD(e.syswm.msg->msg.win.wParam);
                        if (id == IDM_VIEW_SHADERS_NONE)
                            setShader(ShaderType::NONE);
                        if (id == IDM_VIEW_SHADERS_SCANLINES)
                            setShader(ShaderType::SCANLINES);
                        if (id == IDM_VIEW_SHADERS_CRT)
                            setShader(ShaderType::CRT);
                        if (id == IDM_VIEW_SHADERS_CRT3D)
                            setShader(ShaderType::CRT3D);
                        if (id == IDM_VIEW_SHADERS_SCALEFX)
                            setShader(ShaderType::SCALEFX);
                        if (id == IDM_VIEW_SHADERS_XBRZMULTI)
                            setShader(ShaderType::XBRZMULTI);
                        createMenu();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_HACKS_UNLIMITED_SPRITES)
                    {
                        toggleMarkMenuItem(IDM_HACKS_UNLIMITED_SPRITES, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->unlimitedSpritesOff();
                            else
                                this->unlimitedSpritesOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_HACKS_FAST_FORWARD)
                    {
                        toggleMarkMenuItem(IDM_HACKS_FAST_FORWARD, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->fastForwardOff();
                            else
                                this->fastForwardOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_SOUND)
                    {
                        toggleMarkMenuItem(IDM_SOUND_SOUND, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->soundOff();
                            else
                                this->soundOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_PULSE1)
                    {
                        toggleMarkMenuItem(IDM_SOUND_PULSE1, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->pulse1Off();
                            else
                                this->pulse1On(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_PULSE2)
                    {
                        toggleMarkMenuItem(IDM_SOUND_PULSE2, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->pulse2Off();
                            else
                                this->pulse2On(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_TRIANGLE)
                    {
                        toggleMarkMenuItem(IDM_SOUND_TRIANGLE, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->triangleOff();
                            else
                                this->triangleOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_NOISE)
                    {
                        toggleMarkMenuItem(IDM_SOUND_NOISE, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->noiseOff();
                            else
                                this->noiseOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_SOUND_DMC)
                    {
                        toggleMarkMenuItem(IDM_SOUND_DMC, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->dmcOff();
                            else
                                this->dmcOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_INPUT_INVERT_BAYB)
                    {
                        toggleMarkMenuItem(IDM_INPUT_INVERT_BAYB, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->invertBAYBOff();
                            else
                                this->invertBAYBOn(); });
                    }

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_INPUT_USE_ZAPPER)
                    {
                        toggleMarkMenuItem(IDM_INPUT_USE_ZAPPER, [this](bool currentlyChecked)
                                           {
                            if (currentlyChecked)
                                this->useZapperOff();
                            else
                                this->useZapperOn(); });
                    }

                }
#endif
            }

            // Trata o fechamento de janelas individuais
            if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_CLOSE)
                {
                    if (e.window.windowID == SDL_GetWindowID(window))
                    {
                        closed = true;
                    }
                    else if (tileViewer.getWindowID() != 0 && e.window.windowID == tileViewer.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_TILE_VIEWER, false);
                        tileViewer.close();
                        this->tileViewerOpen = false;
                    }
                    else if (paletteViewer.getWindowID() != 0 && e.window.windowID == paletteViewer.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_PALETTE_VIEWER, false);
                        paletteViewer.close();
                        this->paletteViewerOpen = false;
                    }
                    else if (ramViewer.getWindowID() != 0 && e.window.windowID == ramViewer.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_RAM_VIEWER, false);
                        ramViewer.close();
                        this->ramViewerOpen = false;
                    }
                    else if (oamViewer.getWindowID() != 0 && e.window.windowID == oamViewer.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_OAM_VIEWER, false);
                        oamViewer.close();
                        this->oamViewerOpen = false;
                    }
                    else if (vramViewer.getWindowID() != 0 && e.window.windowID == vramViewer.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_VRAM_VIEWER, false);
                        vramViewer.close();
                        this->vramViewerOpen = false;
                    }
                    else if (disassembler.getWindowID() != 0 && e.window.windowID == disassembler.getWindowID())
                    {
                        windowCheckUncheckMenuItem(IDM_DEBUG_DISASSEMBLER, false);
                        disassembler.close();
                        this->disassemblerOpen = false;
                    }
                }
                else if (e.window.event == SDL_WINDOWEVENT_MOVED)
                {
                    // Se a janela principal for movida, reposiciona a Tile Viewer
                    if (e.window.windowID == SDL_GetWindowID(window))
                    {
                        int x, y, w_main, h_main;
                        SDL_GetWindowPosition(window, &x, &y);
                        SDL_GetWindowSize(window, &w_main, &h_main);

                        tileViewer.updatePosition(x, y, w_main);
                        paletteViewer.updatePosition(x, y, w_main);
                        ramViewer.updatePosition(x, y, w_main);
                        oamViewer.updatePosition(x, y, w_main);

                        disassembler.updatePosition(x, y, w_main);
                    }
                    // Update last known windowed position if not in fullscreen
                    if (e.window.windowID == SDL_GetWindowID(window) && currentDisplayMode == DisplayMode::WINDOWED)
                    {
                        lastWindowedX = e.window.data1;
                        lastWindowedY = e.window.data2;
                    }
                }
            }
        }
    }

    void Window::setShader(ShaderType shader)
    {
        this->shader = shader;
    }

    void Window::createMenu()
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            HWND hwnd = wmInfo.info.win.window;

            // Remove e destrói o menu anterior antes de criar um novo (evita memory leaks)
            HMENU hOldMenu = GetMenu(hwnd);
            if (hOldMenu)
                DestroyMenu(hOldMenu);

            // Carrega o ícone do recurso e define na janela
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
            if (hIcon)
            {
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }

            HMENU hMenuBar = CreateMenu();
            HMENU hFileMenu = CreateMenu();
            HMENU hInputMenu = CreateMenu();
            HMENU hDebugMenu = CreateMenu();
            HMENU hDisplayMenu = CreateMenu();
            HMENU hSoundMenu = CreateMenu();
            HMENU hHacksMenu = CreateMenu();

            // Adiciona a opção Open ao menu File
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open ROM...");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_RESET, L"&Reset");
            AppendMenuW(hFileMenu, MF_STRING | (cartLoaded ? MF_ENABLED : MF_DISABLED), IDM_FILE_UNLOAD, L"&Unload");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);

            // Save/Load State
            AppendMenuW(hFileMenu, MF_STRING | (cartLoaded ? MF_ENABLED : MF_DISABLED), IDM_FILE_SAVE, L"&Save State\tF5");
            AppendMenuW(hFileMenu, MF_STRING | (cartLoaded ? MF_ENABLED : MF_DISABLED), IDM_FILE_LOAD, L"&Load State\tF6");

            HMENU hSaveSlotLevelMenu = CreatePopupMenu();
            AppendMenuW(hSaveSlotLevelMenu, MF_STRING | (saveSlot == 1 ? MF_CHECKED : MF_UNCHECKED), IDM_FILE_SAVE_SLOT_1, L"Slot 1");
            AppendMenuW(hSaveSlotLevelMenu, MF_STRING | (saveSlot == 2 ? MF_CHECKED : MF_UNCHECKED), IDM_FILE_SAVE_SLOT_2, L"Slot 2");
            AppendMenuW(hSaveSlotLevelMenu, MF_STRING | (saveSlot == 3 ? MF_CHECKED : MF_UNCHECKED), IDM_FILE_SAVE_SLOT_3, L"Slot 3");

            AppendMenuW(hFileMenu, MF_POPUP | (cartLoaded ? MF_ENABLED : MF_DISABLED), (UINT_PTR)hSaveSlotLevelMenu, L"&Save State Slot");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);

            // Cria o submenu para "Recent Files"
            HMENU hRecentFilesMenu = CreatePopupMenu();
            const auto &recentRoms = configManager.getRecentRoms();

            if (recentRoms.empty())
            {
                AppendMenuW(hRecentFilesMenu, MF_STRING | MF_GRAYED, 0, L"No Recent Files");
            }
            else
            {
                int i = 0;
                for (const auto &romPath : recentRoms)
                {
                    // Mostra apenas o nome do arquivo no menu, para não ficar muito largo
                    std::string fileName = romPath.substr(romPath.find_last_of("\\/") + 1);
                    AppendMenuA(hRecentFilesMenu, MF_STRING, IDM_RECENT_FILE_BASE_ID + i, fileName.c_str());
                    if (++i >= 10)
                        break; // Garante o limite de 10
                }
            }
            AppendMenuW(hFileMenu, MF_POPUP, (UINT_PTR)hRecentFilesMenu, L"&Recent Files");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"&Exit");

            if (this->disassemblerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_DISASSEMBLER, L"&Disassembler");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_DISASSEMBLER, L"&Disassembler");
            }

            if (this->ramViewerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_RAM_VIEWER, L"&RAM Viewer");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_RAM_VIEWER, L"&RAM Viewer");
            }

            AppendMenuW(hDebugMenu, MF_SEPARATOR, 0, NULL);

            if (this->tileViewerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_TILE_VIEWER, L"&Tile Viewer");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_TILE_VIEWER, L"&Tile Viewer");
            }

            if (this->vramViewerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_VRAM_VIEWER, L"&VRAM Viewer");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_VRAM_VIEWER, L"&VRAM Viewer");
            }

            if (this->paletteViewerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_PALETTE_VIEWER, L"&Palette Viewer");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_PALETTE_VIEWER, L"&Palette Viewer");
            }

            if (this->oamViewerOpen)
            {
                AppendMenuW(hDebugMenu, MF_STRING | MF_CHECKED, IDM_DEBUG_OAM_VIEWER, L"&OAM Viewer");
            }
            else
            {
                AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_OAM_VIEWER, L"&OAM Viewer");
            }

            if (this->vsyncEnabled)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_VSYNC, L"&VSync");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_VSYNC, L"&VSync");
            }

            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);

            if (this->currentWindowX == 1)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_WINDOW_1X, L"&1x");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_1X, L"&1x");
            }

            if (this->currentWindowX == 2)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_WINDOW_2X, L"&2x");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_2X, L"&2x");
            }

            if (this->currentWindowX == 3)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_WINDOW_3X, L"&3x");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_3X, L"&3x");
            }

            if (this->currentWindowX == 4)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_WINDOW_4X, L"&4x");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_4X, L"&4x");
            }

            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH, L"&Borderless Fullscreen Stretch");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN, L"&Borderless Fullscreen");

            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);

            if (this->scanlines)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_SCANLINES, L"&Scanlines");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_SCANLINES, L"&Scanlines");
            }

            if (this->cropOverscan)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_CROP_OVERSCAN, L"&Top Crop Overscan (8px)");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_CROP_OVERSCAN, L"&Top Crop Overscan (8px)");
            }

            if (this->fullCropOverscan)
            {
                AppendMenuW(hDisplayMenu, MF_STRING | MF_CHECKED, IDM_VIEW_FULLCROP_OVERSCAN, L"&Full Crop Overscan (8px, 8px, 8px, 8px)");
            }
            else
            {
                AppendMenuW(hDisplayMenu, MF_STRING, IDM_VIEW_FULLCROP_OVERSCAN, L"&Full Crop Overscan (8px, 8px, 8px, 8px)");
            }

            HMENU hScanlineLevelMenu = CreatePopupMenu();
            AppendMenuW(hScanlineLevelMenu, MF_STRING | (scanlinesTransparency == 5 ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SCANLINES_LEVEL_5, L"5%");
            AppendMenuW(hScanlineLevelMenu, MF_STRING | (scanlinesTransparency == 10 ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SCANLINES_LEVEL_10, L"10%");
            AppendMenuW(hScanlineLevelMenu, MF_STRING | (scanlinesTransparency == 15 ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SCANLINES_LEVEL_15, L"15%");
            AppendMenuW(hScanlineLevelMenu, MF_STRING | (scanlinesTransparency == 20 ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SCANLINES_LEVEL_20, L"20%");
            AppendMenuW(hScanlineLevelMenu, MF_STRING | (scanlinesTransparency == 25 ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SCANLINES_LEVEL_25, L"25%");
            AppendMenuW(hDisplayMenu, MF_POPUP, (UINT_PTR)hScanlineLevelMenu, L"&Scanlines Level");

            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);

            HMENU hPalettesLevelMenu = CreatePopupMenu();
            AppendMenuW(hPalettesLevelMenu, MF_STRING | (palettePreset == PaletteType::DEFAULT ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_PALETTE_DEFAULT, L"Default");
            AppendMenuW(hPalettesLevelMenu, MF_STRING | (palettePreset == PaletteType::SMOOTH ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_PALETTE_SMOOTH, L"Smooth");
            AppendMenuW(hPalettesLevelMenu, MF_STRING | (palettePreset == PaletteType::NESTOPIA ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_PALETTE_NESTOPIA, L"Nestopia Emulator");
            AppendMenuW(hPalettesLevelMenu, MF_STRING | (palettePreset == PaletteType::WAVEBEAM ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_PALETTE_WAVEBEAM, L"WaveBeam");
            AppendMenuW(hPalettesLevelMenu, MF_STRING | (palettePreset == PaletteType::NEON ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_PALETTE_NEON, L"Neon");
            AppendMenuW(hDisplayMenu, MF_POPUP, (UINT_PTR)hPalettesLevelMenu, L"&Palettes Preset");

            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);
            HMENU hShadersLevelMenu = CreatePopupMenu();
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::NONE ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_NONE, L"None");
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::SCANLINES ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_SCANLINES, L"Scanlines");
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::CRT ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_CRT, L"CRT");
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::CRT3D ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_CRT3D, L"CRT 3D");
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::SCALEFX ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_SCALEFX, L"ScaleFX");
            AppendMenuW(hShadersLevelMenu, MF_STRING | (shader == ShaderType::XBRZMULTI ? MF_CHECKED : MF_UNCHECKED), IDM_VIEW_SHADERS_XBRZMULTI, L"xBRZ Multi");
            AppendMenuW(hDisplayMenu, MF_POPUP, (UINT_PTR)hShadersLevelMenu, L"&Shaders");

            if (this->soundEnabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_SOUND, L"&Master Sound");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_SOUND, L"&Master Sound");
            }

            AppendMenuW(hSoundMenu, MF_SEPARATOR, 0, NULL);

            if (this->pulse1Enabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_PULSE1, L"&Pulse 1");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_PULSE1, L"&Pulse 1");
            }

            if (this->pulse2Enabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_PULSE2, L"&Pulse 2");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_PULSE2, L"&Pulse 2");
            }

            if (this->triangleEnabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_TRIANGLE, L"&Triangle");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_TRIANGLE, L"&Triangle");
            }

            if (this->noiseEnabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_NOISE, L"&Noise");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_NOISE, L"&Noise");
            }

            if (this->dmcEnabled)
            {
                AppendMenuW(hSoundMenu, MF_STRING | MF_CHECKED, IDM_SOUND_DMC, L"&DMC");
            }
            else
            {
                AppendMenuW(hSoundMenu, MF_STRING, IDM_SOUND_DMC, L"&DMC");
            }

            if (this->invertBAYB)
            {
                AppendMenuW(hInputMenu, MF_STRING | MF_CHECKED, IDM_INPUT_INVERT_BAYB, L"&Invert BA/YB Buttons");
            }
            else
            {
                AppendMenuW(hInputMenu, MF_STRING, IDM_INPUT_INVERT_BAYB, L"&Invert BA/YB Buttons");
            }

            if (this->useZapper)
            {
                AppendMenuW(hInputMenu, MF_STRING | MF_CHECKED, IDM_INPUT_USE_ZAPPER, L"&Enable Zapper");
            }
            else
            {
                AppendMenuW(hInputMenu, MF_STRING, IDM_INPUT_USE_ZAPPER, L"&Enable Zapper");
            }

            if (this->unlimitedSprites)
            {
                AppendMenuW(hHacksMenu, MF_STRING | MF_CHECKED, IDM_HACKS_UNLIMITED_SPRITES, L"&Enable Unlimited Sprites");
            }
            else
            {
                AppendMenuW(hHacksMenu, MF_STRING, IDM_HACKS_UNLIMITED_SPRITES, L"&Enable Unlimited Sprites");
            }

            if (this->fastForwardEnabled)
            {
                AppendMenuW(hHacksMenu, MF_STRING | MF_CHECKED, IDM_HACKS_FAST_FORWARD, L"&Enable Fast Forward");
            }
            else
            {
                AppendMenuW(hHacksMenu, MF_STRING, IDM_HACKS_FAST_FORWARD, L"&Enable Fast Forward");
            }

            // Adiciona o menu File à barra principal
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hDisplayMenu, L"&Display");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hSoundMenu, L"&Sound");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hInputMenu, L"&Input");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hDebugMenu, L"&Debug");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHacksMenu, L"&Hacks");

            SetMenu(hwnd, hMenuBar);
        }
#endif
    }

    void Window::openFileDialog()
    {
#ifdef _WIN32
        OPENFILENAMEA ofn;
        char szFile[260] = {0};
        char szInitialDir[260] = {0};

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "NES ROMs (*.nes, *.zip)\0*.nes;*.zip\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;

        // Set initial directory if available
        std::string lastRomPath = configManager.getLastRomPath();
        if (!lastRomPath.empty() && std::filesystem::is_directory(lastRomPath))
        {
            strncpy(szInitialDir, lastRomPath.c_str(), sizeof(szInitialDir) - 1);
            szInitialDir[sizeof(szInitialDir) - 1] = '\0';
            ofn.lpstrInitialDir = szInitialDir;
        }
        else
        {
            ofn.lpstrInitialDir = NULL;
        }
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            selectedPath = szFile;
            configManager.addRomToList(selectedPath);

            std::filesystem::path romFilePath(selectedPath);
            if (romFilePath.has_parent_path())
            {
                configManager.setLastRomPath(romFilePath.parent_path().string());
            }

            createMenu(); // Atualiza o menu com a nova ROM no topo da lista
        }
#endif
    }

    void Window::openDisassembler()
    {
        this->disassemblerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        disassembler.open(x, y, w);
    }

    void Window::updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status)
    {
        disassembler.render(pc, disassembly, stepByStep, stepRequested, a, x, y, stkp, status);
    }

    void Window::openTileViewer()
    {
        this->tileViewerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        tileViewer.open(x, y, w);
    }

    void Window::openPaletteViewer()
    {
        this->paletteViewerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        paletteViewer.open(x, y, w);
    }

    void Window::updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        tileViewer.render(pixels0, pixels1);
    }

    void Window::updatePaletteViewer(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette)
    {
        paletteViewer.render(paletteTable, systemPalette);
    }

    void Window::openRamViewer()
    {
        this->ramViewerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        ramViewer.open(x, y, w);
    }

    void Window::updateRamViewer(RAM *ram)
    {
        if (ramViewer.isOpen())
        {
            ramViewer.render(ram);
        }
    }

    void Window::openVramViewer()
    {
        this->vramViewerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        vramViewer.open(x, y, w);
    }

    void Window::updateVramViewer(VRAM *vram)
    {
        if (vramViewer.isOpen())
        {
            vramViewer.render(vram);
        }
    }

    void Window::openOamViewer()
    {
        this->oamViewerOpen = true;
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        oamViewer.open(x, y, w);
    }

    void Window::updateOamViewer(const std::array<uint8_t, 256> &oam)
    {
        if (oamViewer.isOpen())
        {
            oamViewer.render(oam);
        }
    }

    void Window::unload()
    {
        unloadRequested = true;
        setPaused(false);
        this->uncheckAllDebugMenuItems();
        this->uncheckZapperMenu();
    }

    void Window::reset()
    {
        resetRequested = true;
        setPaused(false);
        this->uncheckAllDebugMenuItems();
    }

    void Window::uncheckZapperMenu() 
    {
        std::cout << "uncheck zapper menu";
        this->useZapper = false;
        windowCheckUncheckMenuItem(IDM_INPUT_USE_ZAPPER, false);
    }

    void Window::uncheckAllDebugMenuItems()
    {
        this->disassemblerOpen = false;
        this->tileViewerOpen = false;
        this->paletteViewerOpen = false;
        this->ramViewerOpen = false;
        this->oamViewerOpen = false;
        this->vramViewerOpen = false;

        windowCheckUncheckMenuItem(IDM_DEBUG_TILE_VIEWER, false);
        windowCheckUncheckMenuItem(IDM_DEBUG_PALETTE_VIEWER, false);
        windowCheckUncheckMenuItem(IDM_DEBUG_RAM_VIEWER, false);
        windowCheckUncheckMenuItem(IDM_DEBUG_DISASSEMBLER, false);
        windowCheckUncheckMenuItem(IDM_DEBUG_OAM_VIEWER, false);
        windowCheckUncheckMenuItem(IDM_DEBUG_VRAM_VIEWER, false);

        tileViewer.close();
        paletteViewer.close();
        ramViewer.close();
        disassembler.close();
    }

    void Window::setFastForward(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (fastForwardEnabled == enabled)
            return;

        fastForwardEnabled = enabled;

        // Notificar a Engine sobre a mudança
        if (ffCallback)
            ffCallback(fastForwardEnabled);

        std::cout << "Window: Fast Forward " << (fastForwardEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setSound(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (soundEnabled == enabled)
            return;

        soundEnabled = enabled;

        // Notificar a Engine sobre a mudança para ajustar o áudio
        if (soundCallback)
            soundCallback(soundEnabled);

        std::cout << "Window: Sound " << (soundEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setPulse1(bool enabled)
    {
        if (pulse1Enabled == enabled)
            return;

        pulse1Enabled = enabled;

        if (pulse1Callback)
            pulse1Callback(pulse1Enabled);
        std::cout << "Sound: Pulse1 Channel " << (pulse1Enabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setPulse2(bool enabled)
    {
        if (pulse2Enabled == enabled)
            return;

        pulse2Enabled = enabled;

        if (pulse2Callback)
            pulse2Callback(pulse2Enabled);

        std::cout << "Sound: Pulse2 Channel " << (pulse2Enabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setTriangle(bool enabled)
    {
        if (triangleEnabled == enabled)
            return;

        triangleEnabled = enabled;

        if (triangleCallback)
            triangleCallback(triangleEnabled);

        std::cout << "Sound: Triangle Channel " << (triangleEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setNoise(bool enabled)
    {
        if (noiseEnabled == enabled)
            return;

        noiseEnabled = enabled;

        if (noiseCallback)
            noiseCallback(noiseEnabled);

        std::cout << "Sound: Noise Channel " << (noiseEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setDMC(bool enabled)
    {
        if (dmcEnabled == enabled)
            return;

        dmcEnabled = enabled;

        if (dmcCallback)
            dmcCallback(dmcEnabled);

        std::cout << "Sound: DMC Channel " << (dmcEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setUnlimitedSprites(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (unlimitedSprites == enabled)
            return;

        unlimitedSprites = enabled;

        // Notificar a Engine sobre a mudança para ajustar a lógica de renderização
        if (unlimitedSpritesCallback)
            unlimitedSpritesCallback(unlimitedSprites);

        std::cout << "Window: Unlimited Sprites " << (unlimitedSprites ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setInvertBAYB(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (invertBAYB == enabled)
            return;

        invertBAYB = enabled;

        // Notificar a Engine sobre a mudança para ajustar a lógica de renderização
        if (invertBAYBCallback)
            invertBAYBCallback(invertBAYB);

        std::cout << "Window: Inverted BA/YB " << (invertBAYB ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setUseZapper(bool enabled)
    {
        if (useZapper == enabled)
            return;

        useZapper = enabled;

        if (useZapperCallback)
            useZapperCallback(useZapper);
        std::cout << "Input: Use Zapper " << (useZapper ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setVSync(bool enabled)
    {
        // Se não houve mudança e o renderer já existe, não fazemos nada
        if (vsyncEnabled == enabled && renderer != nullptr)
            return;

        vsyncEnabled = enabled;

        // 1. Destruir recursos antigos vinculados ao renderer atual
        if (texture)
        {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        if (renderer)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer2_Shutdown();
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }

        // 2. Criar novo renderer com a flag de VSync se solicitado
        Uint32 flags = SDL_RENDERER_ACCELERATED;
        if (vsyncEnabled)
            flags |= SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(window, -1, flags);

        // 3. Restaurar estado do renderer e recriar texturas
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // 4. Notificar a Engine sobre a mudança para ajustar o Game Loop
        if (vsyncCallback)
            vsyncCallback(vsyncEnabled);

        std::cout << "Window: VSync " << (vsyncEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::windowResize(int times)
    {
        // Exit fullscreen mode if currently in one
        if (currentDisplayMode != DisplayMode::WINDOWED)
        {
            SDL_SetWindowFullscreen(window, 0);      // Set to windowed mode
            SDL_SetWindowBordered(window, SDL_TRUE); // Restore border
            createMenu();                            // Restaura o menu nativo
        }
        
        int currentW = width;
        int currentH = height;
        if (fullCropOverscan) {
            currentW -= 16;
            currentH -= 16;
        } else if (cropOverscan) {
            currentH -= 16;
        }
        SDL_SetWindowSize(window, currentW * times * scale, currentH * times * scale);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        // Update last known windowed size
        SDL_GetWindowSize(window, &lastWindowedW, &lastWindowedH);
        SDL_GetWindowPosition(window, &lastWindowedX, &lastWindowedY);
        currentDisplayMode = DisplayMode::WINDOWED;
        this->currentWindowX = times;
    }

    void Window::setWindowBorderlessFullscreen(DisplayMode dm, Uint32 flags)
    {
        // Save current windowed state before going fullscreen
        if (currentDisplayMode == DisplayMode::WINDOWED)
        {
            SDL_GetWindowPosition(window, &lastWindowedX, &lastWindowedY);
            SDL_GetWindowSize(window, &lastWindowedW, &lastWindowedH);
        }

#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            // Remove o menu do Windows
            SetMenu(wmInfo.info.win.window, NULL);
        }
#endif

        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        currentDisplayMode = dm;
    }

    void Window::windowBorderlessFullscreenStretch()
    {
        this->setWindowBorderlessFullscreen(DisplayMode::FULLSCREEN_STRETCH, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    void Window::windowBorderlessFullscreen()
    {
        this->setWindowBorderlessFullscreen(DisplayMode::FULLSCREEN_ASPECT_8_7, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    void Window::setScanlines(bool enabled)
    {
        if (scanlines == enabled)
            return;

        scanlines = enabled;

        std::cout << "Window: Scanlines " << (scanlines ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::render(const uint32_t *pixels, float fps)
    {
        // Atualiza o título da janela com o FPS
        static float lastFps = -1.0f;
        static bool lastPausedState = false;

        if (fps != lastFps || paused != lastPausedState)
        {
            char titleBuffer[128];
            std::string displayTitle = title + (paused ? " | PAUSED" : "");
            snprintf(titleBuffer, sizeof(titleBuffer), displayTitle.c_str(), paused ? 0.0f : fps);
            SDL_SetWindowTitle(window, titleBuffer);
            lastFps = fps;
            lastPausedState = paused;
        }

        const uint32_t *finalPixels = pixels;

        if (scanlines)
        {
            if (postProcessBuffer.size() != width * height)
                postProcessBuffer.resize(width * height);

            float factor = 1.0f - (scanlinesTransparency / 100.0f);

            for (int y = 0; y < height; ++y)
            {
                bool isScanlineRow = (y % 2 != 0);
                uint32_t rowOffset = y * width;

                for (int x = 0; x < width; ++x)
                {
                    uint32_t index = rowOffset + x;
                    uint32_t pixel = pixels[index];

                    if (isScanlineRow)
                    {
                        // Aplica o fator de escurecimento mantendo o canal Alpha
                        pixel = (pixel & 0xFF000000) |
                                (static_cast<uint32_t>(((pixel >> 16) & 0xFF) * factor) << 16) |
                                (static_cast<uint32_t>(((pixel >> 8) & 0xFF) * factor) << 8) |
                                (static_cast<uint32_t>((pixel & 0xFF) * factor));
                    }
                    postProcessBuffer[index] = pixel;
                }
            }
            finalPixels = postProcessBuffer.data();
        }

        SDL_UpdateTexture(texture, nullptr, finalPixels, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);

        // Define a área de origem da textura (Source Rect)
        // Se o Crop estiver ativado, pulamos os primeiros 8 scanlines e reduzimos a altura em 16 (8 topo + 8 base)
        SDL_Rect src_rect = {0, 0, width, height};
        if (fullCropOverscan)
        {
            src_rect.x = 8;
            src_rect.y = 8;
            src_rect.w = width - 16;
            src_rect.h = height - 16;
        }
        else if (cropOverscan)
        {
            src_rect.y = 8;
            src_rect.h = height - 16;
        }


        SDL_Rect dest_rect;
        if (currentDisplayMode == DisplayMode::FULLSCREEN_ASPECT_8_7)
        {
            int current_window_w, current_window_h;
            SDL_GetWindowSize(window, &current_window_w, &current_window_h);

            // Se cortamos 16 pixels de altura (8 topo + 8 base), para manter o mesmo "look" dos pixels
            // (Pixel Aspect Ratio), precisamos ajustar a proporção da tela (Display Aspect Ratio).
            // 8:7 original assume 240 linhas. Para 224 linhas, a nova proporção é 60:49.
            double target_aspect_ratio;
            if (fullCropOverscan) {
                // 240x224 -> 60:49
                target_aspect_ratio = 60.0 / 49.0;
            } else if (cropOverscan) {
                // 256x224 -> 8:7
                target_aspect_ratio = 8.0 / 7.0;
            } else {
                target_aspect_ratio = 8.0 / 7.0;
            }

            int render_w, render_h;
            double potential_h = current_window_w / target_aspect_ratio;
            double potential_w = current_window_h * target_aspect_ratio;

            if (potential_h <= current_window_h)
            {
                render_w = static_cast<int>(current_window_w);
                render_h = static_cast<int>(potential_h);
            }
            else
            {
                render_w = static_cast<int>(potential_w);
                render_h = static_cast<int>(current_window_h);
            }

            dest_rect.w = render_w;
            dest_rect.h = render_h;
            dest_rect.x = (current_window_w - render_w) / 2;
            dest_rect.y = (current_window_h - render_h) / 2;

            SDL_RenderCopy(renderer, texture, &src_rect, &dest_rect);
        }
        else
        {
            SDL_RenderCopy(renderer, texture, &src_rect, nullptr);
        }

        // Renderiza o overlay de PAUSE em modo Fullscreen
        if (paused && currentDisplayMode != DisplayMode::WINDOWED && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Posiciona no centro da tela
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("PauseOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::SetWindowFontScale(4.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), "PAUSED");
            ImGui::End();

            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        }

        SDL_RenderPresent(renderer);
    }
}