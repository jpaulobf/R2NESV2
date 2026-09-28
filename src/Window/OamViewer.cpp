#include "OamViewer.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <string>
namespace R2NES::Core
{
    // Construtor — nada é criado até `open()` ser chamado.
    OamViewer::OamViewer() {}

    // Destrutor — encerra os backends ImGui e destrói os objetos SDL se presentes.
    OamViewer::~OamViewer()
    {
        if (window)
        {
            // Restaura o contexto ImGui desta instância antes de encerrar os backends.
            if (imguiContext)
                ImGui::SetCurrentContext(imguiContext);

            // Encerra os backends ImGui SDL/Renderer e destrói renderer/janela.
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);

            // Destroi o contexto ImGui dedicado.
            if (imguiContext)
                ImGui::DestroyContext(imguiContext);
        }
    }

    // Cria e inicializa a janela SDL, o renderer e o contexto ImGui.
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

            // Cria um contexto ImGui separado para que esta janela possa ser
            // mostrada/ocultada independentemente da UI principal.
            imguiContext = ImGui::CreateContext();
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
            ImGui_ImplSDLRenderer2_Init(renderer);
            visible = true;
        }
    }

    // Oculta a janela (mantemos os recursos alocados para reabertura rápida).
    void OamViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t OamViewer::getWindowID() const { return window ? SDL_GetWindowID(window) : 0; }

    // Encaminha eventos de entrada ao ImGui deste visualizador.
    void OamViewer::handleEvent(SDL_Event *e)
    {
        if (visible && window && imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDL2_ProcessEvent(e);
        }
    }

    // Mantém a janela posicionada ao lado da janela pai.
    void OamViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX + parentW, parentY);
    }

    // Renderiza a tabela OAM em um table ImGui e apresenta usando o renderer SDL.
    void OamViewer::render(const std::array<uint8_t, 256> &oam)
    {
        if (!visible || !renderer || !imguiContext)
            return;

        // Usa o contexto ImGui deste visualizador e inicia um novo frame.
        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // UI cobrindo toda a janela para apresentar os dados do OAM.
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

            // Cada sprite ocupa 4 bytes: Y, Tile, Attr, X
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
                // os dois bits menos significativos de attr geralmente codificam o índice de paleta
                ImGui::Text("%d", attr & 0x03);

                ImGui::TableSetColumnIndex(4);
                // Monta uma pequena string de flags a partir dos bits do atributo (V/H/P)
                std::string flags = "";
                if (attr & 0x80)
                    flags += "V"; // flip vertical
                else
                    flags += "-";
                if (attr & 0x40)
                    flags += "H"; // flip horizontal
                else
                    flags += "-";
                if (attr & 0x20)
                    flags += "P"; // seleção de paleta / bit de prioridade
                else
                    flags += "-";
                ImGui::Text("%s", flags.c_str());

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("$%02X", x);
            }
            ImGui::EndTable();
        }

        ImGui::End();

        // Renderiza ImGui no renderer SDL e apresenta o resultado.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }
}