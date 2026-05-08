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

        // Abre ou restaura a janela do Tile Viewer
        void open(int parentX, int parentY, int parentW);
        void close();

        // Renderização dos Pattern Tables
        void render(const uint32_t *pixels0, const uint32_t *pixels1);

        void handleEvent(SDL_Event *e); // Pode ser útil para futuros eventos específicos da janela
        void updatePosition(int parentX, int parentY, int parentW);

        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *tileTexture[2] = {nullptr, nullptr};
        bool visible = false;
    };
}