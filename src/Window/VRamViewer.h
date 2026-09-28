#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>
#include "imgui.h"

namespace R2NES::Core
{
    class VRAM;

    class VRamViewer
    {
    public:
        VRamViewer();
        ~VRamViewer();

        // Abre ou restaura a janela do visualizador de VRAM.
        // parentX/parentY/parentW: posição e largura da janela pai, usados para posicionamento relativo.
        void open(int parentX, int parentY, int parentW);

        // Fecha (oculta) a janela mantendo recursos para reuso rápido.
        void close();

        // Renderiza o conteúdo da VRAM dentro de uma janela ImGui.
        // Recebe ponteiro para o objeto `VRAM` para leitura/escrita direta.
        void render(VRAM *vram);

        // Encaminha eventos SDL para o contexto ImGui desta janela.
        void handleEvent(SDL_Event *e);

        // Atualiza a posição da janela relativa ao pai.
        void updatePosition(int parentX, int parentY, int parentW);

        // Estado e identificação da janela
        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;         // Janela SDL do visualizador
        SDL_Renderer *renderer = nullptr;     // Renderer SDL usado para apresentar ImGui
        ImGuiContext *imguiContext = nullptr; // Contexto ImGui isolado para esta janela

        bool visible = false; // Bandeira de visibilidade/abertura
    };
}