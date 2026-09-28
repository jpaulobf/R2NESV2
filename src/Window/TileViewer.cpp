#include "TileViewer.h"

namespace R2NES::Core
{
    TileViewer::TileViewer() {}

    TileViewer::~TileViewer()
    {
        if (window)
        {
            // Destroi texturas, renderer e janela ao encerrar.
            if (tileTexture[0])
                SDL_DestroyTexture(tileTexture[0]);
            if (tileTexture[1])
                SDL_DestroyTexture(tileTexture[1]);
            if (renderer)
                SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
    }

    void TileViewer::open(int parentX, int parentY, int parentW)
    {
        // Se já existe, apenas traz a janela de volta à vista.
        if (window)
        {
            SDL_ShowWindow(window);
            visible = true;
            return;
        }

        // Uma Pattern Table é 128x128. Exibiremos as duas lado a lado (256x128).
        // Aplicamos uma escala de 2x para facilitar a visualização (512x256).
        window = SDL_CreateWindow(
            "R2NES v2 - Pattern Table Viewer",
            parentX + parentW, parentY,
            512, 256,
            SDL_WINDOW_SHOWN);

        if (window)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

            // Define a cor de fundo como preto e limpa a janela imediatamente
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);

            // Criamos duas texturas (uma para cada Pattern Table de 128x128).
            // As texturas usam formato ARGB8888 e serão atualizadas por `SDL_UpdateTexture`.
            tileTexture[0] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 128, 128);
            tileTexture[1] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 128, 128);

            visible = true;
        }
    }

    void TileViewer::close()
    {
        if (window)
        {
            SDL_HideWindow(window);
            visible = false;
        }
    }

    uint32_t TileViewer::getWindowID() const
    {
        return window ? SDL_GetWindowID(window) : 0;
    }

    void TileViewer::handleEvent(SDL_Event *e)
    {
        // Atualmente não processamos eventos internos do SDL/ImGui aqui.
        // A responsabilidade de lidar com fechamento ou foco é da `Window`.
    }

    void TileViewer::updatePosition(int parentX, int parentY, int parentW)
    {
        if (window)
            SDL_SetWindowPosition(window, parentX + parentW, parentY);
    }

    void TileViewer::render(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        if (!visible || !renderer)
            return;
        // Atualiza as texturas com os pixels fornecidos. Assumimos arrays de 128x128
        // cada, em 32-bit ARGB. O pitch (bytes por linha) é 128 * sizeof(uint32_t).
        SDL_UpdateTexture(tileTexture[0], nullptr, pixels0, 128 * sizeof(uint32_t));
        SDL_UpdateTexture(tileTexture[1], nullptr, pixels1, 128 * sizeof(uint32_t));

        // Limpa o renderer e desenha as duas texturas lado a lado.
        SDL_RenderClear(renderer);

        // Destinos dimensionados para aplicar escala 2x (128->256)
        SDL_Rect dest0 = {0, 0, 256, 256};
        SDL_Rect dest1 = {256, 0, 256, 256};

        SDL_RenderCopy(renderer, tileTexture[0], nullptr, &dest0);
        SDL_RenderCopy(renderer, tileTexture[1], nullptr, &dest1);

        // Apresenta o frame na tela.
        SDL_RenderPresent(renderer);
    }
}