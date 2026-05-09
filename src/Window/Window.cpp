#include "Window.h"
#include <iostream>
#include <SDL_syswm.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

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
#define IDM_DEBUG_TILE_VIEWER 1005
#define IDM_DEBUG_DISASSEMBLER 1006
#define IDM_VIEW_VSYNC 1999
#define IDM_VIEW_WINDOW_1X 2000
#define IDM_VIEW_WINDOW_2X 2001
#define IDM_VIEW_WINDOW_3X 2002
#define IDM_VIEW_WINDOW_4X 2003
#define IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN 2004
#define IDM_VIEW_WINDOW_BORDERLESS_FULLSCREEN_STRETCH 2005
#define IDM_HACKS_UNLIMITED_SPRITES 3000
#define IDM_HACKS_FAST_FORWARD 3001

namespace R2NES::Core
{
    Window::Window(const std::string &title, int w, int h, int s)
        : width(w), height(h), scale(s)
    {
        // Store initial windowed state
        lastWindowedW = w * s;
        lastWindowedH = h * s;
        // lastWindowedX and Y are already initialized to SDL_WINDOWPOS_CENTERED

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
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
        ImGui::CreateContext();

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

        ImGui::DestroyContext();

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void Window::pollEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            // Só processa eventos do ImGui se a janela de debug estiver ativa
            tileViewer.handleEvent(&e);
            disassembler.handleEvent(&e);

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
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_TILE_VIEWER)
                    {
                        openTileViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_DEBUG_DISASSEMBLER)
                    {
                        openDisassembler();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_VIEW_VSYNC)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        UINT state = GetMenuState(hMenu, IDM_VIEW_VSYNC, MF_BYCOMMAND);

                        if (state & MF_CHECKED)
                        {
                            CheckMenuItem(hMenu, IDM_VIEW_VSYNC, MF_BYCOMMAND | MF_UNCHECKED);
                            this->vsyncOff();
                        }
                        else
                        {
                            CheckMenuItem(hMenu, IDM_VIEW_VSYNC, MF_BYCOMMAND | MF_CHECKED);
                            this->vsyncOn();
                        }
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

                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_HACKS_UNLIMITED_SPRITES)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        UINT state = GetMenuState(hMenu, IDM_HACKS_UNLIMITED_SPRITES, MF_BYCOMMAND);

                        if (state & MF_CHECKED)
                        {
                            CheckMenuItem(hMenu, IDM_HACKS_UNLIMITED_SPRITES, MF_BYCOMMAND | MF_UNCHECKED);
                            this->unlimitedSpritesOff();
                        }
                        else
                        {
                            CheckMenuItem(hMenu, IDM_HACKS_UNLIMITED_SPRITES, MF_BYCOMMAND | MF_CHECKED);
                            this->unlimitedSpritesOn();
                        }
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_HACKS_FAST_FORWARD)
                    {
                        HMENU hMenu = GetMenu(e.syswm.msg->msg.win.hwnd);
                        UINT state = GetMenuState(hMenu, IDM_HACKS_FAST_FORWARD, MF_BYCOMMAND);

                        if (state & MF_CHECKED)
                        {
                            CheckMenuItem(hMenu, IDM_HACKS_FAST_FORWARD, MF_BYCOMMAND | MF_UNCHECKED);
                            this->fastForwardOff();
                        }
                        else
                        {
                            CheckMenuItem(hMenu, IDM_HACKS_FAST_FORWARD, MF_BYCOMMAND | MF_CHECKED);
                            this->fastForwardOn();
                        }
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
                        tileViewer.close();
                    }
                    else if (disassembler.getWindowID() != 0 && e.window.windowID == disassembler.getWindowID())
                    {
                        disassembler.close();
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

    void Window::createMenu()
    {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window, &wmInfo))
        {
            HWND hwnd = wmInfo.info.win.window;
            HMENU hMenuBar = CreateMenu();
            HMENU hFileMenu = CreateMenu();
            HMENU hDebugMenu = CreateMenu();
            HMENU hDisplayMenu = CreateMenu();
            HMENU hHacksMenu = CreateMenu();

            // Adiciona a opção Open ao menu File
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open ROM...");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_RESET, L"&Reset");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_UNLOAD, L"&Unload");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"&Exit");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_TILE_VIEWER, L"&Tile Viewer");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_DEBUG_DISASSEMBLER, L"&Disassembler");
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

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "NES ROMs (*.nes, *.zip)\0*.nes;*.zip\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            selectedPath = szFile;
        }
#endif
    }

    void Window::openDisassembler()
    {
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
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        tileViewer.open(x, y, w);
    }

    void Window::updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        tileViewer.render(pixels0, pixels1);
    }

    void Window::unload()
    {
        unloadRequested = true;

        // Fecha as janelas de debug ao descarregar a ROM
        tileViewer.close();

        disassembler.close();
    }

    void Window::reset()
    {
        resetRequested = true;

        // Fecha/Esconde as janelas de debug para um reset "limpo"
        tileViewer.close();

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
        SDL_SetWindowSize(window, width * times * scale, height * times * scale);
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

        // This makes the window fill the entire screen, borderless.
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

    void Window::render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                        bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status, float fps)
    {
        // Atualiza o título da janela com o FPS
        static float lastFps = -1.0f;
        if (fps != lastFps)
        {
            char titleBuffer[64];
            snprintf(titleBuffer, sizeof(titleBuffer), "R2NESV2 - build 0.3.2 | FPS: %.2f", fps);
            SDL_SetWindowTitle(window, titleBuffer);
            lastFps = fps;
        }

        SDL_UpdateTexture(texture, nullptr, pixels, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);

        SDL_Rect dest_rect;
        if (currentDisplayMode == DisplayMode::FULLSCREEN_ASPECT_8_7)
        {
            int current_window_w, current_window_h;
            SDL_GetWindowSize(window, &current_window_w, &current_window_h);

            double target_aspect_ratio = 8.0 / 7.0; // Desired 8:7 aspect ratio for NES

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

            SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
        }
        else
        {
            // Default behavior for windowed modes or fullscreen stretch
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        }
        SDL_RenderPresent(renderer);
    }
}