#pragma once

#include "Core/IO/NESButtons.h"
#include "KeyboardMapping.h"
#include <SDL3/SDL.h>
#include <map>
#include <cstdint>

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
        void handleJoystick(int playerNum, SDL_GamepadButton button, bool isPressed, Core::NES &nes);
        void setPlayerJoystick(int player, bool enabled, Core::NES &nes);

        // Configura a associação dos botões físicos A/B/X/Y e seus atalhos turbo.
        void configureABBAButtons(bool invert);

        // Libera as teclas anteriores e aplica os mapeamentos dos dois controles.
        void setKeyboardMapping(const R2Keyboard::Mapping &mapping, Core::NES &nes);

        // Seleciona Gamepad ou Zapper como dispositivo da segunda porta do NES.
        void configureUseZapper(bool enabled, Core::NES &nes);

        // Atualiza os pulsos de turbo de acordo com o quadro atual da emulação.
        void update(Core::NES &nes, uint64_t emulatedFrameCount);

    private:
        // Aplica botões e turbo do primeiro controle físico.
        void handleJoystick1(SDL_GamepadButton button, bool isPressed, Core::NES &nes);

        // Aplica botões do segundo controle físico.
        void handleJoystick2(SDL_GamepadButton button, bool isPressed, Core::NES &nes);

        // Mapeamento de teclas para o Player 1 e 2
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player2KeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player1TurboKeyMap;
        std::map<SDL_Keycode, R2NES::Core::IO::NESButtons> player2TurboKeyMap;
        R2Keyboard::Mapping keyboardMapping = R2Keyboard::defaults();
        // Mantém o turbo do segundo controle separado das pressões normais.
        bool turbo2A = false, turbo2B = false;
        bool normal2A = false, normal2B = false, turboPulse2 = false;
        // Recria as associações após mudar as teclas ou a inversão do primeiro controle.
        void rebuildKeyboardMap();

        // Mapeamento de botões de controle
        std::map<SDL_GamepadButton, R2NES::Core::IO::NESButtons> player1ControllerMap;
        std::map<SDL_GamepadButton, R2NES::Core::IO::NESButtons> player2ControllerMap;
        std::map<SDL_GamepadButton, R2NES::Core::IO::NESButtons> player1TurboControllerMap;

        // Mantêm os botões turbo pressionados entre os pulsos gerados em update().
        bool turboA = false;
        bool turboB = false;

        // Registram a configuração ativa de layout e dispositivo da segunda porta.
        bool playerJoystick[2] = {false, false};
        bool invertBAYB = false;
        bool useZapper = false;
    };
}