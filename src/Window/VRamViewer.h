#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>

namespace R2NES::Core
{
    class VRamViewer
    {
    public:
        VRamViewer();
        ~VRamViewer();

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        
        bool visible = false;
    };
}