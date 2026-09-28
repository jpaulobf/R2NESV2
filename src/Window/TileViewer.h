#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>

namespace R2NES::Core
{
    class TileViewer
    {
    public:
        TileViewer();
        ~TileViewer();

        // Abre ou restaura a janela do Tile Viewer.
        // parentX/parentY/parentW: posição e largura da janela "pai" para posicionamento relativo.
        void open(int parentX, int parentY, int parentW);

        // Fecha (oculta) a janela mantendo recursos para reuso rápido.
        void close();

        // Renderiza as duas Pattern Tables (cada uma 128x128 pixels) fornecidas como
        // arrays de pixels ARGB (32-bit). `pixels0` corresponde à Pattern Table 0,
        // `pixels1` à Pattern Table 1. A janela aplica escala 2x ao apresentar.
        void render(const uint32_t *pixels0, const uint32_t *pixels1);

        // Processamento de eventos SDL local — atualmente não utilizado, mas reservado
        // para futuras interações específicas da janela (ex.: atalhos, cliques).
        void handleEvent(SDL_Event *e);

        // Atualiza a posição da janela relativa ao pai (mantém o viewer ao lado do pai).
        void updatePosition(int parentX, int parentY, int parentW);

        // Verifica se a janela está visível/aberta e obtém o SDL Window ID.
        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;                     // Janela SDL do visualizador
        SDL_Renderer *renderer = nullptr;                 // Renderer associado à janela
        SDL_Texture *tileTexture[2] = {nullptr, nullptr}; // Texturas para cada Pattern Table
        bool visible = false;                             // Estado de visibilidade
    };
}