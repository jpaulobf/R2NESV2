#pragma once

#include <SDL.h>
#include <cstdint>
#include <array>
#include "imgui.h"

namespace R2NES::Core
{
    class OamViewer
    {
    public:
        OamViewer();
        ~OamViewer();

        void open(int parentX, int parentY, int parentW);
        void close();
        void render(const std::array<uint8_t, 256> &oam);
        void handleEvent(SDL_Event *e);
        void updatePosition(int parentX, int parentY, int parentW);

        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        ImGuiContext *imguiContext = nullptr;
        bool visible = false;

        struct SpriteData
        {
            uint8_t y;
            uint8_t tile;
            uint8_t attrs;
            uint8_t x;
        };
    };
}