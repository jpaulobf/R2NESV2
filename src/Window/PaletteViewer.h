#pragma once

#include <SDL.h>
#include <cstdint>
#include <array>
#include "imgui.h"

namespace R2NES::Core
{
    class PaletteViewer
    {
    public:
        PaletteViewer();
        ~PaletteViewer();

        void open(int parentX, int parentY, int parentW);
        void close();
        void render(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette);
        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;
        void handleEvent(SDL_Event *e);
        void updatePosition(int parentX, int parentY, int parentW);

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        ImGuiContext *imguiContext = nullptr;
        bool visible = false;
    };
}