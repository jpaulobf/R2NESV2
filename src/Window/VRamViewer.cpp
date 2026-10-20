#include "VRamViewer.h"
#include "ScopedImGuiContext.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
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
            if (imguiContext)
            {
                ScopedImGuiContext scope(imguiContext);
                ImGui_ImplSDLRenderer3_Shutdown();
                ImGui_ImplSDL3_Shutdown();
            }
            if (renderer)
                SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            if (imguiContext)
                ImGui::DestroyContext(imguiContext);
        }
    }

    void VRamViewer::open(int parentX, int parentY, int parentW)
    {
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }
        // Cria a janela posicionada à esquerda do pai e um pouco abaixo
        window = SDL_CreateWindow("R2NES v2 - VRAM Viewer", 512, 400, SDL_WINDOW_RESIZABLE);
        if (window)
        {
            // Renderer acelerado para apresentar os comandos do ImGui
            renderer = SDL_CreateRenderer(window, nullptr);
            if (!renderer)
            {
                SDL_DestroyWindow(window);
                window = nullptr;
                return;
            }
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);

            imguiContext = ImGui::CreateContext();
            {
                ScopedImGuiContext scope(imguiContext);
                ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
                ImGui_ImplSDLRenderer3_Init(renderer);
            }
            visible = true;
        }
    }

    void VRamViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t VRamViewer::getWindowID() const { return window ? SDL_GetWindowID(window) : 0; }

    void VRamViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            uint32_t winId = getEventWindowID(e);
            if (winId == 0 || winId == SDL_GetWindowID(window))
            {
                ScopedImGuiContext scope(imguiContext);
                ImGui_ImplSDL3_ProcessEvent(e);
            }
        }
    }

    void VRamViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX - 512, parentY + 300);
    }

    void VRamViewer::render(const MemoryAccess &memory)
    {
        if (!visible || !renderer || !imguiContext || !memory.read || memory.size == 0 || memory.size > 0x10000)
            return;

        ScopedImGuiContext scope(imguiContext);
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("VRAM Viewer", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        // Área rolável com a visualização em hexdump + ASCII, linhas de 16 bytes.
        ImGui::BeginChild("VRAM_ScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 4), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Text("Addr | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ASCII");
        ImGui::Separator();

        for (std::size_t addr = 0; addr < memory.size; addr += 16)
        {
            ImGui::Text("%04X |", static_cast<unsigned>(addr));
            ImGui::SameLine();
            std::string ascii_line;
            for (int i = 0; i < 16 && addr + i < memory.size; ++i)
            {
                uint8_t byte_value = memory.read(static_cast<uint16_t>(addr + i));
                ImGui::Text("%02X", byte_value);
                ImGui::SameLine();
                ascii_line += (byte_value >= 0x20 && byte_value <= 0x7E) ? (char)byte_value : '.';
            }
            ImGui::Text("| %s", ascii_line.c_str());
        }
        ImGui::EndChild();

        // Controles de edição simples: endereço e valor em hex, com validação de faixa.
        static uint16_t editAddr = 0x0000;
        static uint8_t editValue = 0x00;
        ImGui::Separator();
        ImGui::Text("Edit VRAM Byte (Offset 0x000 - 0x7FF):");
        ImGui::InputScalar("Address", ImGuiDataType_U16, &editAddr, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Value", ImGuiDataType_U8, &editValue, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        ImGui::BeginDisabled(!memory.write);
        if (ImGui::Button("Write") && editAddr < memory.size)
        {
            memory.write(editAddr, editValue);
        }
        ImGui::EndDisabled();

        ImGui::End();

        // Render ImGui para a janela SDL
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
}