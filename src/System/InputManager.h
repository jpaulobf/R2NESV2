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
    // Traduz eventos SDL para os botões dos dois controles do NES e para a Zapper.
    class InputManager
    {
    public:
        // Cria os mapeamentos padrão de teclado, controle e turbo.
        InputManager();
        ~InputManager() = default;

        // Aplica um evento de teclado ao primeiro controle do NES.
        void handleKeyboard(SDL_Keycode key, bool isPressed, Core::NES &nes);

        // Direciona um evento de controle SDL para o jogador correspondente.
        void handleJoystick(int playerNum, SDL_GameControllerButton button, bool isPressed, Core::NES &nes);

        // Configura a associação dos botões físicos A/B/X/Y e seus atalhos turbo.
        void configureABBAButtons(bool invert);

        // Seleciona Gamepad ou Zapper como dispositivo da segunda porta do NES.
        void configureUseZapper(bool enabled, Core::NES &nes);

        // Atualiza os pulsos de turbo de acordo com o quadro atual da emulação.
        void update(Core::NES &nes, int frameCount);

    private:
        // Aplica botões e turbo do primeiro controle físico.
        void handleJoystick1(SDL_GameControllerButton button, bool isPressed, Core::NES &nes);

        // Aplica botões do segundo controle físico.
        void handleJoystick2(SDL_GameControllerButton button, bool isPressed, Core::NES &nes);

        // Mapeamento de teclas para o Player 1 e 2
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player2KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1TurboKeyMap;

        // Mapeamento de botões de controle
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player1ControllerMap;
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player2ControllerMap;
        std::map<SDL_GameControllerButton, R2NES::Core::IO::NESButtons> player1TurboControllerMap;

        // Mantêm os botões turbo pressionados entre os pulsos gerados em update().
        bool turboA = false;
        bool turboB = false;

        // Registram a configuração ativa de layout e dispositivo da segunda porta.
        bool invertBAYB = false;
        bool useZapper = false;
    };
}
