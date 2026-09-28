#include "InputManager.h"
#include "Core/NES.h"

namespace R2NES::System
{
    // Define os controles padrão de teclado, gamepad e turbo do jogador 1.
    InputManager::InputManager()
    {
        // Inicializa o mapeamento de teclas padrão para o Player 1
        player1KeyMap[SDLK_j] = R2NES::Core::IO::BUTTON_B;
        player1KeyMap[SDLK_k] = R2NES::Core::IO::BUTTON_A;
        player1KeyMap[SDLK_BACKSPACE] = R2NES::Core::IO::BUTTON_SELECT;
        player1KeyMap[SDLK_RETURN] = R2NES::Core::IO::BUTTON_START;
        player1KeyMap[SDLK_w] = R2NES::Core::IO::BUTTON_UP;
        player1KeyMap[SDLK_s] = R2NES::Core::IO::BUTTON_DOWN;
        player1KeyMap[SDLK_a] = R2NES::Core::IO::BUTTON_LEFT;
        player1KeyMap[SDLK_d] = R2NES::Core::IO::BUTTON_RIGHT;

        // Mapeamento de Turbo (Teclado)
        player1TurboKeyMap[SDLK_i] = R2NES::Core::IO::BUTTON_A;
        player1TurboKeyMap[SDLK_u] = R2NES::Core::IO::BUTTON_B;

        // Mapeamento de Controles padrão
        player1ControllerMap[SDL_CONTROLLER_BUTTON_BACK] = R2NES::Core::IO::BUTTON_SELECT;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_START] = R2NES::Core::IO::BUTTON_START;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_UP] = R2NES::Core::IO::BUTTON_UP;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = R2NES::Core::IO::BUTTON_DOWN;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = R2NES::Core::IO::BUTTON_LEFT;
        player1ControllerMap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = R2NES::Core::IO::BUTTON_RIGHT;

        player2ControllerMap = player1ControllerMap;

        configureABBAButtons(false);
    }

    // Reconfigura os botões de ação e remove atalhos turbo deixados pelo layout anterior.
    void InputManager::configureABBAButtons(bool invert)
    {
        invertBAYB = invert;

        // Limpa mapeamentos de Turbo anteriores para evitar estados residuais ao alternar
        player1TurboControllerMap.erase(SDL_CONTROLLER_BUTTON_A);
        player1TurboControllerMap.erase(SDL_CONTROLLER_BUTTON_B);
        player1TurboControllerMap.erase(SDL_CONTROLLER_BUTTON_X);
        player1TurboControllerMap.erase(SDL_CONTROLLER_BUTTON_Y);

        if (invert)
        {
            player1ControllerMap[SDL_CONTROLLER_BUTTON_A] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_B] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_X] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_Y] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[SDL_CONTROLLER_BUTTON_Y] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[SDL_CONTROLLER_BUTTON_B] = R2NES::Core::IO::BUTTON_A;
        }
        else
        {
            player1ControllerMap[SDL_CONTROLLER_BUTTON_A] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_B] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_X] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[SDL_CONTROLLER_BUTTON_Y] = R2NES::Core::IO::BUTTON_A;
            player1TurboControllerMap[SDL_CONTROLLER_BUTTON_X] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[SDL_CONTROLLER_BUTTON_Y] = R2NES::Core::IO::BUTTON_A;
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
