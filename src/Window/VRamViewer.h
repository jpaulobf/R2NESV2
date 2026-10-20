#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include "imgui.h"
#include "MemoryAccess.h"

namespace R2NES::Core
{
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
        // Utiliza os callbacks de leitura/escrita apenas durante esta chamada.
        void render(const MemoryAccess &memory);

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