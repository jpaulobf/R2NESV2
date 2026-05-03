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
#define IDM_FILE_TILE_VIEWER 1003
#define IDM_FILE_DISASSEMBLER 1004

namespace R2NES::Core
{
    Window::Window(const std::string &title, int w, int h, int s)
        : width(w), height(h), scale(s)
    {
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
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_EXIT)
                    {
                        closed = true;
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_TILE_VIEWER)
                    {
                        openTileViewer();
                    }
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == IDM_FILE_DISASSEMBLER)
                    {
                        openDisassembler();
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

            // Adiciona a opção Open ao menu File
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open ROM...");
            AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"&Exit");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_FILE_TILE_VIEWER, L"&Tile Viewer");
            AppendMenuW(hDebugMenu, MF_STRING, IDM_FILE_DISASSEMBLER, L"&Disassembler");

            // Adiciona o menu File à barra principal
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
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

    void Window::render(const uint32_t *pixels, uint16_t pc, const std::map<uint16_t, std::string> &disassembly, bool &stepByStep, bool &stepRequested)
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
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // Controles de depuração
            ImGui::Checkbox("Step-by-Step", &stepByStep);
            ImGui::SameLine();
            if (ImGui::Button("Next Instruction"))
                stepRequested = true;

            ImGui::Separator();

            static uint16_t lastPC = 0;
            bool pcChanged = (pc != lastPC);

            for (auto const &[addr, line] : disassembly)
            {
                // Se o endereço da linha for o PC atual, desenhamos em uma cor diferente (ex: Ciano)
                if (addr == pc)
                {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), ">> %s", line.c_str());

                    // Faz o scroll automático apenas quando o PC mudar (emulação rodando)
                    if (pcChanged)
                    {
                        ImGui::SetScrollHereY(0.5f);
                    }
                }
                else
                {
                    ImGui::Text("   %s", line.c_str());
                }
            }

            lastPC = pc;
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
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}