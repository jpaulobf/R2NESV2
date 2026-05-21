#include "RamViewer.h"
#include "Core/Memory/RAM/RAM.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace R2NES::Core
{
    RamViewer::RamViewer() {}

    RamViewer::~RamViewer()
    {
        if (window)
        {
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            if (imguiContext)
            {
                ImGui::DestroyContext(imguiContext);
                imguiContext = nullptr;
            }
            if (renderer)
                SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    void RamViewer::open(int parentX, int parentY, int parentW)
    {
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }

        window = SDL_CreateWindow(
            "R2NES v2 - RAM Viewer",
            parentX - 512, parentY,
            512, 600,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

        if (window)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (renderer)
            {
                // Define a cor de fundo como preto e limpa a janela imediatamente após a criação
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderPresent(renderer);

                imguiContext = ImGui::CreateContext();
                ImGui::SetCurrentContext(imguiContext);

                ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
                ImGui_ImplSDLRenderer2_Init(renderer);
                visible = true;
            }
            else
            {
                SDL_DestroyWindow(window);
                window = nullptr;
            }
        }
    }

    void RamViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t RamViewer::getWindowID() const
    {
        return window ? SDL_GetWindowID(window) : 0;
    }

    void RamViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_ProcessEvent(e);
        }
    }

    void RamViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX - 512, parentY);
    }

    void RamViewer::render(RAM *ram)
    {
        if (!visible || !renderer || !ram || !imguiContext)
            return;

        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(512, 600));
        ImGui::Begin("RAM Viewer", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        size_t ramSize = ram->getSize();

        static uint16_t startAddr = 0x0000;
        static uint16_t endAddr = 0x07FF;

        ImGui::InputScalar("Start Address", ImGuiDataType_U16, &startAddr, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("End Address", ImGuiDataType_U16, &endAddr, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);

        if (startAddr > endAddr)
            std::swap(startAddr, endAddr);
        if (startAddr >= ramSize)
            startAddr = static_cast<uint16_t>(ramSize - 1);
        if (endAddr >= ramSize)
            endAddr = static_cast<uint16_t>(ramSize - 1);

        ImGui::BeginChild("RAM_ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Text("Addr | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ASCII");
        ImGui::Separator();

        for (uint16_t addr = startAddr; addr <= endAddr; addr += 16)
        {
            ImGui::Text("%04X |", addr);
            ImGui::SameLine();

            std::string ascii_line;
            for (int i = 0; i < 16; ++i)
            {
                uint16_t current_byte_addr = addr + i;
                if (current_byte_addr <= endAddr)
                {
                    uint8_t byte_value = ram->read(current_byte_addr);
                    ImGui::Text("%02X", byte_value);
                    ImGui::SameLine();

                    if (byte_value >= 0x20 && byte_value <= 0x7E)
                        ascii_line += (char)byte_value;
                    else
                        ascii_line += '.';
                }
                else
                {
                    ImGui::Text("  ");
                    ImGui::SameLine();
                    ascii_line += ' ';
                }
            }
            ImGui::Text("| %s", ascii_line.c_str());
        }

        static uint16_t editAddr = 0x0000;
        static uint8_t editValue = 0x00;
        ImGui::Separator();
        ImGui::Text("Edit Byte:");
        ImGui::InputScalar("Address", ImGuiDataType_U16, &editAddr, NULL, NULL, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Value", ImGuiDataType_U8, &editValue, NULL, NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("Write") && editAddr < ramSize)
        {
            ram->write(editAddr, editValue);
        }

        ImGui::EndChild();
        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}