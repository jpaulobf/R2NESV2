#pragma once

#include "Core/IO/NESButtons.h"
#include <SDL.h>
#include <map>

namespace R2NES::Core
{
    class NES;
}

namespace R2NES::System
{
    class InputManager
    {
    public:
        InputManager();
        ~InputManager() = default;

        void handleKeyboard(SDL_Keycode key, bool isPressed, Core::NES &nes);
        void handleJoystick(int playerNum, SDL_GameControllerButton button, bool isPressed, Core::NES &nes);

        void configureABBAButtons(bool invert);
        void configureUseZapper(bool enabled, Core::NES &nes);

        void update(Core::NES &nes, int frameCount);

    private:
        void handleJoystick1(SDL_GameControllerButton button, bool isPressed, Core::NES &nes);
        void handleJoystick2(SDL_GameControllerButton button, bool isPressed, Core::NES &nes);

        // Mapeamento de teclas para o Player 1 e 2
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player2KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1TurboKeyMap;

        // Mapeamento de botões de controle
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player1ControllerMap;
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player2ControllerMap;
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player1TurboControllerMap;

        bool turboA = false;
        bool turboB = false;
        bool invertBAYB = false;
        bool useZapper = false;
    };
}
