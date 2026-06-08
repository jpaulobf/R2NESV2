#include "VRamViewer.h"
#include "Core/Memory/VRAM/VRAM.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace R2NES::Core
{
    VRamViewer::VRamViewer() {}

    VRamViewer::~VRamViewer()
    {
        if (window)
        {
            if (imguiContext) ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            if (imguiContext) ImGui::DestroyContext(imguiContext);
            if (renderer) SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    void VRamViewer::open(int parentX, int parentY, int parentW)
    {
        if (window) { SDL_ShowWindow(window); visible = true; return; }

        window = SDL_CreateWindow("R2NES v2 - VRAM Viewer", parentX - 512, parentY + 300, 512, 400, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (window)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
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

    void VRamViewer::close() { if (window) { SDL_HideWindow(window); visible = false; } }

    uint32_t VRamViewer::getWindowID() const { return window ? SDL_GetWindowID(window) : 0; }

    void VRamViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_ProcessEvent(e);
        }
    }

    void VRamViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window) SDL_SetWindowPosition(window, parentX - 512, parentY + 300);
    }

    void VRamViewer::render(VRAM *vram)
    {
        if (!visible || !renderer || !vram || !imguiContext) return;

        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("VRAM Viewer", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        ImGui::BeginChild("VRAM_ScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 4), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Text("Addr | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ASCII");
        ImGui::Separator();

        for (uint16_t addr = 0; addr < vram->getSize(); addr += 16)
        {
            ImGui::Text("%04X |", addr); ImGui::SameLine();
            std::string ascii_line;
            for (int i = 0; i < 16; ++i)
            {
                uint8_t byte_value = vram->readRaw(addr + i);
                ImGui::Text("%02X", byte_value); ImGui::SameLine();
                ascii_line += (byte_value >= 0x20 && byte_value <= 0x7E) ? (char)byte_value : '.';
            }
            ImGui::Text("| %s", ascii_line.c_str());
        }
        ImGui::EndChild();

        static uint16_t editAddr = 0x0000;
        static uint8_t editValue = 0x00;
        ImGui::Separator();
        ImGui::Text("Edit VRAM Byte (Offset 0x000 - 0x7FF):");
        ImGui::InputScalar("Address", ImGuiDataType_U16, &editAddr, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Value", ImGuiDataType_U8, &editValue, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("Write") && editAddr < vram->getSize())
        {
            vram->writeRaw(editAddr, editValue);
        }

        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}