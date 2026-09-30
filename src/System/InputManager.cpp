#include "InputManager.h"
#include "Core/NES.h"

namespace R2NES::System
{
    SDL_KeyCode defaultKeyMap[] = {SDLK_j, SDLK_k, SDLK_i, SDLK_u, SDLK_BACKSPACE, SDLK_RETURN, SDLK_w, SDLK_s, SDLK_a, SDLK_d};
    SDL_GameControllerButton defaultControllerMap[] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT};

    // Define os controles padrão de teclado, gamepad e turbo do jogador 1.
    InputManager::InputManager()
    {
        // Inicializa o mapeamento de teclas padrão para o Player 1
        player1KeyMap[defaultKeyMap[0]] = R2NES::Core::IO::BUTTON_B;
        player1KeyMap[defaultKeyMap[1]] = R2NES::Core::IO::BUTTON_A;
        player1KeyMap[defaultKeyMap[4]] = R2NES::Core::IO::BUTTON_SELECT;
        player1KeyMap[defaultKeyMap[5]] = R2NES::Core::IO::BUTTON_START;
        player1KeyMap[defaultKeyMap[6]] = R2NES::Core::IO::BUTTON_UP;
        player1KeyMap[defaultKeyMap[7]] = R2NES::Core::IO::BUTTON_DOWN;
        player1KeyMap[defaultKeyMap[8]] = R2NES::Core::IO::BUTTON_LEFT;
        player1KeyMap[defaultKeyMap[9]] = R2NES::Core::IO::BUTTON_RIGHT;

        // Mapeamento de Turbo (Teclado)
        player1TurboKeyMap[defaultKeyMap[2]] = R2NES::Core::IO::BUTTON_A;
        player1TurboKeyMap[defaultKeyMap[3]] = R2NES::Core::IO::BUTTON_B;

        // Mapeamento de Controles padrão
        player1ControllerMap[defaultControllerMap[4]] = R2NES::Core::IO::BUTTON_SELECT;
        player1ControllerMap[defaultControllerMap[5]] = R2NES::Core::IO::BUTTON_START;
        player1ControllerMap[defaultControllerMap[6]] = R2NES::Core::IO::BUTTON_UP;
        player1ControllerMap[defaultControllerMap[7]] = R2NES::Core::IO::BUTTON_DOWN;
        player1ControllerMap[defaultControllerMap[8]] = R2NES::Core::IO::BUTTON_LEFT;
        player1ControllerMap[defaultControllerMap[9]] = R2NES::Core::IO::BUTTON_RIGHT;

        player2ControllerMap = player1ControllerMap;
        configureABBAButtons(false);
    }

    // Reconfigura os botões de ação e remove atalhos turbo deixados pelo layout anterior.
    void InputManager::configureABBAButtons(bool invert)
    {
        invertBAYB = invert;

        // Limpa mapeamentos de Turbo anteriores para evitar estados residuais ao alternar
        player1TurboControllerMap.erase(defaultControllerMap[0]);
        player1TurboControllerMap.erase(defaultControllerMap[1]);
        player1TurboControllerMap.erase(defaultControllerMap[2]);
        player1TurboControllerMap.erase(defaultControllerMap[3]);

        player1KeyMap.erase(defaultKeyMap[0]);
        player1KeyMap.erase(defaultKeyMap[1]);
        player1KeyMap.erase(defaultKeyMap[2]);
        player1KeyMap.erase(defaultKeyMap[3]);
        player1TurboKeyMap.erase(defaultKeyMap[0]);
        player1TurboKeyMap.erase(defaultKeyMap[1]);
        player1TurboKeyMap.erase(defaultKeyMap[2]);
        player1TurboKeyMap.erase(defaultKeyMap[3]);

        if (invert)
        {
            player1ControllerMap[defaultControllerMap[0]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1KeyMap[defaultKeyMap[3]] = R2NES::Core::IO::BUTTON_B;
            player1KeyMap[defaultKeyMap[0]] = R2NES::Core::IO::BUTTON_A;
            player1TurboKeyMap[defaultKeyMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1TurboKeyMap[defaultKeyMap[2]] = R2NES::Core::IO::BUTTON_B;
        }
        else
        {
            player1ControllerMap[defaultControllerMap[0]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_A;
            player1TurboControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_A;
            player1KeyMap[defaultKeyMap[0]] = R2NES::Core::IO::BUTTON_B;
            player1KeyMap[defaultKeyMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1TurboKeyMap[defaultKeyMap[2]] = R2NES::Core::IO::BUTTON_A;
            player1TurboKeyMap[defaultKeyMap[3]] = R2NES::Core::IO::BUTTON_B;
        }
    }

    // Atualiza a porta 2 do NES para o dispositivo selecionado no menu.
    void InputManager::configureUseZapper(bool enabled, Core::NES &nes)
    {
        useZapper = enabled;

        if (enabled)
        {
            nes.getJoysticks().port2Device = R2NES::Core::IO::DeviceType::Zapper;
        }
        else
        {
            nes.getJoysticks().port2Device = R2NES::Core::IO::DeviceType::Gamepad;
        }
    }

    // Processa teclas de jogo e mantém o estado de turbo separado do botão normal.
    void InputManager::handleKeyboard(SDL_Keycode key, bool isPressed, Core::NES &nes)
    {
        auto &joy1 = nes.getJoysticks().controller1;

        auto it = player1KeyMap.find(key);
        if (it != player1KeyMap.end())
        {
            joy1.setButton(it->second, isPressed);
        }

        auto itTurbo = player1TurboKeyMap.find(key);
        if (itTurbo != player1TurboKeyMap.end())
        {
            if (itTurbo->second == R2NES::Core::IO::BUTTON_A)
                turboA = isPressed;
            if (itTurbo->second == R2NES::Core::IO::BUTTON_B)
                turboB = isPressed;

            if (!isPressed)
                joy1.setButton(itTurbo->second, false);
        }
    }

    // Direciona o evento do gamepad ao controlador emulado do jogador correto.
    void InputManager::handleJoystick(int playerNum, SDL_GameControllerButton button, bool isPressed, Core::NES &nes)
    {
        if (playerNum == 1)
        {
            handleJoystick1(button, isPressed, nes);
        }
        else if (playerNum == 2)
        {
            handleJoystick2(button, isPressed, nes);
        }
    }

    // Atualiza o controle 1 e ativa ou desativa os botões turbo associados.
    void InputManager::handleJoystick1(SDL_GameControllerButton button, bool isPressed, Core::NES &nes)
    {
        auto &joy1 = nes.getJoysticks().controller1;
        auto it = player1ControllerMap.find(button);
        if (it != player1ControllerMap.end())
        {
            joy1.setButton(it->second, isPressed);
        }

        auto itTurbo = player1TurboControllerMap.find(button);
        if (itTurbo != player1TurboControllerMap.end())
        {
            if (itTurbo->second == R2NES::Core::IO::BUTTON_A)
                turboA = isPressed;
            if (itTurbo->second == R2NES::Core::IO::BUTTON_B)
                turboB = isPressed;

            if (!isPressed)
                joy1.setButton(itTurbo->second, false);
        }
    }

    // Atualiza exclusivamente os botões convencionais do segundo controle.
    void InputManager::handleJoystick2(SDL_GameControllerButton button, bool isPressed, Core::NES &nes)
    {
        auto &joy2 = nes.getJoysticks().controller2;
        auto it = player2ControllerMap.find(button);
        if (it != player2ControllerMap.end())
        {
            joy2.setButton(it->second, isPressed);
        }
    }

    // Alterna A e B turbo a cada dois quadros enquanto seus atalhos permanecem pressionados.
    void InputManager::update(Core::NES &nes, int frameCount)
    {
        auto &joy1 = nes.getJoysticks().controller1;
        bool turboPulse = (frameCount % 4 > 2); // Fica 'true' por 2 frames, 'false' por 2 frames

        if (turboA)
            joy1.setButton(R2NES::Core::IO::BUTTON_A, turboPulse);
        if (turboB)
            joy1.setButton(R2NES::Core::IO::BUTTON_B, turboPulse);
    }
}
