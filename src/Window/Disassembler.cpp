#include "Disassembler.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

namespace R2NES::Core
{
    Disassembler::Disassembler() {}

    Disassembler::~Disassembler()
    {
        if (window)
        {
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    void Disassembler::open(int parentX, int parentY, int parentW)
    {
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }

        window = SDL_CreateWindow(
            "R2NES v2 - Disassembler",
            parentX + parentW, parentY + 300,
            512, 300,
            SDL_WINDOW_SHOWN);

        if (window)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
            ImGui_ImplSDLRenderer2_Init(renderer);
            visible = true;
        }
    }

    void Disassembler::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t Disassembler::getWindowID() const
    {
        return window ? SDL_GetWindowID(window) : 0;
    }

    void Disassembler::handleEvent(SDL_Event *e)
    {
        if (visible && window)
            ImGui_ImplSDL2_ProcessEvent(e);
    }

    void Disassembler::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX + parentW, parentY + 300);
    }

    void Disassembler::render(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                              bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status)
    {
        if (!visible || !renderer)
            return;

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(512, 300));
        ImGui::Begin("Disassembler", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Checkbox("Step-by-Step", &stepByStep);
        ImGui::SameLine();
        if (ImGui::Button("Next Instruction"))
            stepRequested = true;
        ImGui::Separator();

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 360.0f);

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
        ImGui::NewLine();
        showFlag("D", 0x08);
        showFlag("I", 0x04);
        showFlag("Z", 0x02);
        showFlag("C", 0x01);
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::Text("P (HEX): $%02X", status);
        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}