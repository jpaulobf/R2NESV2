#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>
#include "imgui.h"

namespace R2NES::Core
{
    class RAM;

    class RamViewer
    {
    public:
        RamViewer();
        ~RamViewer();

        void open(int parentX, int parentY, int parentW);
        
        void close();

        void render(RAM *ram);

        void handleEvent(SDL_Event *e);

        void updatePosition(int parentX, int parentY, int parentW);

        bool isOpen() const { return visible; }

        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        bool visible = false;
        ImGuiContext *imguiContext = nullptr;
    };
}