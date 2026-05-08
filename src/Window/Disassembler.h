#pragma once

#include <SDL.h>
#include <string>
#include <map>
#include <cstdint>

namespace R2NES::Core
{
    class Disassembler
    {
    public:
        Disassembler();
        ~Disassembler();

        // Abre ou restaura a janela do Disassembler
        void open(int parentX, int parentY, int parentW);
        void close();

        // Renderização da interface ImGui
        void render(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);

        void handleEvent(SDL_Event *e);
        void updatePosition(int parentX, int parentY, int parentW);

        bool isOpen() const { return visible; }
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        bool visible = false;
    };
}