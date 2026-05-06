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
#define IDM_FILE_TILE_VIEWER 1005
#define IDM_FILE_DISASSEMBLER 1006
#define IDM_FILE_WINDOW_1X 2000
#define IDM_FILE_WINDOW_2X 2001
#define IDM_FILE_WINDOW_3X 2002
#define IDM_FILE_WINDOW_4X 2003
#define IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN 2004
#define IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN_STRETCH 2005

namespace R2NES::Core
{
    Window::Window(const std::string &title, int w, int h, int s)
        : width(w), height(h), scale(s)
    {
        // Store initial windowed state
        lastWindowedW = w * s;
        lastWindowedH = h * s;
        // lastWindowedX and Y are already initialized to SDL_WINDOWPOS_CENTERED

        if (SDL_Init(SDL_INIT_VIDEO) < 0)
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

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

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

        this->windowResize(3);
    }

    Window::~Window()
    {
        if (tileWindow)
        {
            SDL_DestroyTexture(tileTexture[0]);
            SDL_DestroyTexture(tileTexture[1]);
            SDL_DestroyRenderer(tileRenderer);
            SDL_DestroyWindow(tileWindow);
        }
        if (disasmWindow)
        {
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            SDL_DestroyRenderer(disasmRenderer);
            SDL_DestroyWindow(disasmWindow);
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
            if (showDisasm && disasmWindow)
                ImGui_ImplSDL2_ProcessEvent(&e);

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
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_TILE_VIEWER)
                    {
                        openTileViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_DISASSEMBLER)
                    {
                        openDisassembler();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_1X)
                    {
                        windowResize(1);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_2X)
                    {
                        windowResize(2);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_3X)
                    {
                        windowResize(3);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_4X)
                    {
                        windowResize(4);
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN_STRETCH)
                    {
                        windowBorderlessFullscreenStretch();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN)
                    {
                        windowBorderlessFullscreen();
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
                    else if (tileWindow && e.window.windowID == SDL_GetWindowID(tileWindow))
                    {
                        SDL_HideWindow(tileWindow);
                        tileViewerOpen = false;
                    }
                    else if (disasmWindow && e.window.windowID == SDL_GetWindowID(disasmWindow))
                    {
                        SDL_HideWindow(disasmWindow);
                        showDisasm = false;
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

                        if (tileWindow)
                            SDL_SetWindowPosition(tileWindow, x + w_main, y);

                        if (disasmWindow)
                            SDL_SetWindowPosition(disasmWindow, x + w_main, y + 300);
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

            // Adiciona a opção Open ao menu File
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open ROM...");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_RESET, L"&Reset");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_UNLOAD, L"&Unload");
            AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"&Exit");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_FILE_TILE_VIEWER, L"&Tile Viewer");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_FILE_DISASSEMBLER, L"&Disassembler");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_1X, L"&1x");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_2X, L"&2x");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_3X, L"&3x");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_4X, L"&4x");
            AppendMenuW(hDisplayMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN_STRETCH, L"&Borderless Fullscreen Stretch");
            AppendMenuW(hDisplayMenu, MF_STRING, IDM_FILE_WINDOW_BORDERLESS_FULLSCREEN, L"&Borderless Fullscreen");

            // Adiciona o menu File à barra principal
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hDisplayMenu, L"&Display");
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hDebugMenu, L"&Debug");

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
        ofn.lpstrFilter = "NES ROMs (*.nes)\0*.nes\0All Files (*.*)\0*.*\0";
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
        if (disasmWindow)
        {
            SDL_ShowWindow(disasmWindow);
            showDisasm = true;
            return;
        }

        // Pega a posição da janela principal para abrir o disasm embaixo dela
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        disasmWindow = SDL_CreateWindow(
            "R2NES v2 - Disassembler",
            x + w, y + 300, // Abre logo abaixo da principal
            512, 300,
            SDL_WINDOW_SHOWN);

        if (disasmWindow)
        {
            disasmRenderer = SDL_CreateRenderer(disasmWindow, -1, SDL_RENDERER_ACCELERATED);

            // Inicializa o ImGui especificamente para o Renderer desta janela
            ImGui_ImplSDL2_InitForSDLRenderer(disasmWindow, disasmRenderer);
            ImGui_ImplSDLRenderer2_Init(disasmRenderer);

            showDisasm = true;
        }
    }

    void Window::openTileViewer()
    {
        if (tileWindow)
        {
            SDL_ShowWindow(tileWindow);
            tileViewerOpen = true;
            return;
        }

        // Pega a posição e tamanho da janela principal para colar ao lado
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        // Uma Pattern Table é 128x128. Exibiremos as duas lado a lado (256x128).
        // Aplicamos uma escala de 2x para facilitar a visualização (512x256).
        tileWindow = SDL_CreateWindow(
            "R2NES v2 - Pattern Table Viewer",
            x + w, y,
            512, 256,
            SDL_WINDOW_SHOWN);

        if (tileWindow)
        {
            tileRenderer = SDL_CreateRenderer(tileWindow, -1, SDL_RENDERER_ACCELERATED);

            // Define a cor de fundo como preto e limpa a janela imediatamente
            SDL_SetRenderDrawColor(tileRenderer, 0x00, 0x00, 0x00, 0xFF);
            SDL_RenderClear(tileRenderer);
            SDL_RenderPresent(tileRenderer);

            // Criamos duas texturas (uma para cada Pattern Table de 128x128)
            tileTexture[0] = SDL_CreateTexture(tileRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 128, 128);
            tileTexture[1] = SDL_CreateTexture(tileRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 128, 128);

            tileViewerOpen = true;
        }
    }

    void Window::updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        if (!tileViewerOpen || !tileRenderer)
            return;

        SDL_UpdateTexture(tileTexture[0], nullptr, pixels0, 128 * sizeof(uint32_t));
        SDL_UpdateTexture(tileTexture[1], nullptr, pixels1, 128 * sizeof(uint32_t));

        SDL_RenderClear(tileRenderer);

        SDL_Rect dest0 = {0, 0, 256, 256};
        SDL_Rect dest1 = {256, 0, 256, 256};

        SDL_RenderCopy(tileRenderer, tileTexture[0], nullptr, &dest0);
        SDL_RenderCopy(tileRenderer, tileTexture[1], nullptr, &dest1);

        SDL_RenderPresent(tileRenderer);
    }

    void Window::unload()
    {
        unloadRequested = true;

        // Fecha as janelas de debug ao descarregar a ROM
        if (tileWindow)
        {
            SDL_HideWindow(tileWindow);
            tileViewerOpen = false;
        }
        if (disasmWindow)
        {
            SDL_HideWindow(disasmWindow);
            showDisasm = false;
        }
    }

    void Window::reset()
    {
        resetRequested = true;

        // Fecha/Esconde as janelas de debug para um reset "limpo"
        if (tileWindow)
        {
            SDL_HideWindow(tileWindow);
            tileViewerOpen = false;
        }
        if (disasmWindow)
        {
            SDL_HideWindow(disasmWindow);
            showDisasm = false;
        }
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
    }

    void Window::windowBorderlessFullscreenStretch()
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
            // Remove o menu do Windows para um visual verdadeiramente "borderless"
            SetMenu(wmInfo.info.win.window, NULL);
        }
#endif

        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        currentDisplayMode = DisplayMode::FULLSCREEN_STRETCH;
    }

    void Window::windowBorderlessFullscreen()
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

        // Set to borderless fullscreen desktop mode
        // This makes the window fill the entire screen, borderless.
        // The rendering logic in `render` will then handle the 8:7 aspect ratio.
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        currentDisplayMode = DisplayMode::FULLSCREEN_ASPECT_8_7;
    }

    void Window::render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                        bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status)
    {
        if (showDisasm)
        {
            // Inicializa o frame apenas para a janela secundária
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Agora a janela ImGui preenche toda a janela SDL secundária
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(512, 300));

            ImGui::Begin("Disassembler", &showDisasm,
                         ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove);

            // Controles de depuração
            ImGui::Checkbox("Step-by-Step", &stepByStep);
            ImGui::SameLine();
            if (ImGui::Button("Next Instruction"))
                stepRequested = true;

            ImGui::Separator();

            // Divide a janela: Coluna 0 para Código, Coluna 1 para Estado da CPU
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 360.0f);

            // --- Lado Esquerdo: Disassembly (com scroll independente) ---
            ImGui::BeginChild("CodeScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

            static uint16_t lastPC = 0;
            bool pcChanged = (pc != lastPC);

            for (auto const &[addr, line] : disassembly)
            {
                if (addr == pc)
                {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), ">> %s", line.c_str());

                    if (pcChanged)
                        ImGui::SetScrollHereY(0.5f);
                }
                else
                {
                    ImGui::Text("   %s", line.c_str());
                }
            }

            lastPC = pc;
            ImGui::EndChild();

            ImGui::NextColumn();

            // --- Lado Direito: Registradores e Flags ---
            ImGui::Text("Registers");
            ImGui::Separator();
            ImGui::Text("A:  $%02X", a);
            ImGui::Text("X:  $%02X", x);
            ImGui::Text("Y:  $%02X", y);
            ImGui::Text("PC: $%04X", pc);
            ImGui::Text("SP: $%02X", stkp);

            ImGui::Spacing();
            ImGui::Text("Flags (NVUBDIZC)");
            ImGui::Separator();

            // Helper para mostrar flags coloridas (Verde se 1, Cinza se 0)
            auto showFlag = [&](const char *label, uint8_t bit)
            {
                bool set = status & bit;
                ImGui::TextColored(set ? ImVec4(0, 1, 0, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1), "%s", label);
                ImGui::SameLine();
            };

            showFlag("N", 0x80);
            showFlag("V", 0x40);
            showFlag("U", 0x20);
            showFlag("B", 0x10);
            ImGui::NewLine(); // Quebra linha para não ficar muito largo
            showFlag("D", 0x08);
            showFlag("I", 0x04);
            showFlag("Z", 0x02);
            showFlag("C", 0x01);

            ImGui::NewLine();
            ImGui::Separator();

            // Status bruto em Hex pra conferência rápida
            ImGui::Text("P (HEX): $%02X", status);

            ImGui::Columns(1); // Volta para coluna única
            ImGui::End();

            // Renderiza no renderer do Disassembler
            SDL_SetRenderDrawColor(disasmRenderer, 0, 0, 0, 255);
            SDL_RenderClear(disasmRenderer);
            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
            SDL_RenderPresent(disasmRenderer);
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