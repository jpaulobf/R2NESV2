#pragma once

#include <SDL.h>
#include <string>
#include <map>
#include "imgui.h"
#include <cstdint>

namespace R2NES::Core
{
    // Exibe o desassembly e o estado da CPU em uma janela SDL/ImGui independente.
    class Disassembler
    {
    public:
        // Cria o viewer sem alocar a janela até o primeiro pedido de abertura.
        Disassembler();

        // Libera o contexto ImGui, renderer e janela auxiliares.
        ~Disassembler();

        // Cria ou restaura a janela do Disassembler ao lado da janela principal.
        void open(int parentX, int parentY, int parentW);

        // Oculta a janela sem descartar seus recursos SDL e ImGui.
        void close();

        // Renderiza instruções próximas ao PC, registradores, flags e controles de passo a passo.
        void render(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);

        // Encaminha eventos SDL ao contexto ImGui próprio enquanto o viewer está visível.
        void handleEvent(SDL_Event *e);

        // Mantém a janela ancorada à posição da janela principal.
        void updatePosition(int parentX, int parentY, int parentW);

        // Informa se o viewer está visível para a Window e o menu nativo.
        bool isOpen() const { return visible; }

        // Retorna o identificador SDL da janela ou zero quando ela ainda não existe.
        uint32_t getWindowID() const;

    private:
        // Recursos exclusivos da janela auxiliar e seu contexto ImGui isolado.
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        bool visible = false;
        ImGuiContext *imguiContext = nullptr;
    };
}