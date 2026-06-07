#include "PaletteViewer.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <cstdio>

namespace R2NES::Core
{
    PaletteViewer::PaletteViewer() {}

    PaletteViewer::~PaletteViewer()
    {
        if (window)
        {
            if (imguiContext)
                ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            if (imguiContext)
                ImGui::DestroyContext(imguiContext);
        }
    }

    void PaletteViewer::open(int parentX, int parentY, int parentW)
    {
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }

        window = SDL_CreateWindow("R2NES v2 - Palette Viewer", parentX + parentW, parentY + 600, 430, 160, SDL_WINDOW_SHOWN);
        if (window)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

            // Define a cor de fundo como preto e limpa a janela imediatamente para evitar o fundo branco
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);

            imguiContext = ImGui::CreateContext();
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
            ImGui_ImplSDLRenderer2_Init(renderer);
            visible = true;
        }
    }

    void PaletteViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t PaletteViewer::getWindowID() const { return window ? SDL_GetWindowID(window) : 0; }

    void PaletteViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_ProcessEvent(e);
        }
    }

    void PaletteViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX + parentW, parentY + 600);
    }

    void PaletteViewer::render(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette)
    {
        if (!visible || !renderer || !imguiContext)
            return;

        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(430, 160));
        ImGui::Begin("Palettes", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        auto drawPalettes = [&](const char *label, int offset)
        {
            ImGui::Text("%s", label);
            for (int p = 0; p < 4; p++)
            {
                for (int i = 0; i < 4; i++)
                {
                    int addr = offset + (p * 4) + i;
                    // Aplicar lógica de espelhamento do NES ($3F10, $3F14, $3F18, $3F1C -> $3F00, $3F04, $3F08, $3F0C)
                    int effectiveAddr = addr;
                    if ((effectiveAddr & 0x13) == 0x10)
                        effectiveAddr &= 0x0F;

                    uint8_t colorIdx = paletteTable[effectiveAddr] & 0x3F;
                    uint32_t color = systemPalette[colorIdx];

                    // Converter ARGB (0xFFRRGGBB) para ABGR (0xFFBBGGRR) para o ImU32 do ImGui
                    ImU32 imguiColor = 0xFF000000 |
                                       ((color & 0x00FF0000) >> 16) |
                                       (color & 0x0000FF00) |
                                       ((color & 0x000000FF) << 16);

                    char id[16];
                    sprintf(id, "##%s_%d_%d", label, p, i);
                    ImGui::ColorButton(id, ImGui::ColorConvertU32ToFloat4(imguiColor),
                                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18, 18));

                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Entry: $%02X\nValue: $%02X", addr + 0x3F00, colorIdx);

                    if (i < 3)
                        ImGui::SameLine();
                }
                if (p < 3)
                    ImGui::SameLine(0, 10);
            }
        };

        drawPalettes("Background Palettes", 0);
        ImGui::Dummy(ImVec2(0, 5));
        drawPalettes("Sprite Palettes", 16);

        ImGui::End();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}