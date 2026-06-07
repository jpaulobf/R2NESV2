#include "OamViewer.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <string>

namespace R2NES::Core
{
    OamViewer::OamViewer() {}

    OamViewer::~OamViewer()
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

    void OamViewer::open(int parentX, int parentY, int parentW)
    {
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }

        window = SDL_CreateWindow("R2NES v2 - OAM Viewer", parentX + parentW, parentY, 400, 500, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

    void OamViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t OamViewer::getWindowID() const { return window ? SDL_GetWindowID(window) : 0; }

    void OamViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_ProcessEvent(e);
        }
    }

    void OamViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX + parentW, parentY);
    }

    void OamViewer::render(const std::array<uint8_t, 256> &oam)
    {
        if (!visible || !renderer || !imguiContext)
            return;

        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("OAM Data", &visible, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (ImGui::BeginTable("OAMTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 35.0f);
            ImGui::TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Pal", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 35.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < 64; i++)
            {
                uint8_t y = oam[i * 4 + 0];
                uint8_t tile = oam[i * 4 + 1];
                uint8_t attr = oam[i * 4 + 2];
                uint8_t x = oam[i * 4 + 3];

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%02d", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("$%02X", y);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("$%02X", tile);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", attr & 0x03);

                ImGui::TableSetColumnIndex(4);
                std::string flags = "";
                if (attr & 0x80)
                    flags += "V";
                else
                    flags += "-";
                if (attr & 0x40)
                    flags += "H";
                else
                    flags += "-";
                if (attr & 0x20)
                    flags += "P";
                else
                    flags += "-";
                ImGui::Text("%s", flags.c_str());

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("$%02X", x);
            }
            ImGui::EndTable();
        }

        ImGui::End();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}