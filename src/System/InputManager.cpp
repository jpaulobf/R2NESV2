#include "InputManager.h"
#include "Core/NES.h"

namespace R2NES::System
{
    SDL_GamepadButton defaultControllerMap[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT};

    // Define os controles padrão de teclado, gamepad e turbo do jogador 1.
    InputManager::InputManager()
    {
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

        rebuildKeyboardMap();

        if (invert)
        {
            player1ControllerMap[defaultControllerMap[0]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
        }
        else
        {
            player1ControllerMap[defaultControllerMap[0]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[1]] = R2NES::Core::IO::BUTTON_A;
            player1ControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1ControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_A;
            player1TurboControllerMap[defaultControllerMap[2]] = R2NES::Core::IO::BUTTON_B;
            player1TurboControllerMap[defaultControllerMap[3]] = R2NES::Core::IO::BUTTON_A;
        }
    }

    void InputManager::rebuildKeyboardMap()
    {
        using namespace R2NES::Core::IO;
        player1KeyMap.clear();
        player2KeyMap.clear();
        player1TurboKeyMap.clear();
        player2TurboKeyMap.clear();
        constexpr NESButtons buttons[] = {BUTTON_UP, BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT,
                                          BUTTON_B, BUTTON_A, BUTTON_B, BUTTON_A, BUTTON_START, BUTTON_SELECT};
        // A inversão afeta o primeiro controle e preserva a configuração armazenada.
        constexpr int inverted[] = {0, 1, 2, 3, 5, 7, 4, 6, 8, 9};
        for (int p = 0; p < 2; ++p)
            for (int i = 0; i < 10; ++i)
            {
                const auto key = keyboardMapping[p][i];
                const int target = invertBAYB && p == 0 ? inverted[i] : i;
                if (!key)
                    continue;
                auto &map = target == 6 || target == 7
                                ? (p == 0 ? player1TurboKeyMap : player2TurboKeyMap)
                                : (p == 0 ? player1KeyMap : player2KeyMap);
                map[key] = buttons[target];
            }
    }

    void InputManager::setKeyboardMapping(const R2Keyboard::Mapping &mapping, Core::NES &nes)
    {
        // Solta as associações antigas antes de reconstruir os mapas de teclado.
        for (const auto &pad : keyboardMapping)
            for (const auto key : pad)
                if (key)
                    handleKeyboard(key, false, nes);
        keyboardMapping = mapping;
        rebuildKeyboardMap();
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
        auto &joy2 = nes.getJoysticks().controller2;
        // Pressões normais do Pad 2 prevalecem quando o pulso turbo está desligado.
        const auto normal2 = player2KeyMap.find(key);
        if (normal2 != player2KeyMap.end())
        {
            if (normal2->second == R2NES::Core::IO::BUTTON_A)
            {
                normal2A = isPressed;
                joy2.setButton(normal2->second, normal2A || (turbo2A && turboPulse2));
            }
            else if (normal2->second == R2NES::Core::IO::BUTTON_B)
            {
                normal2B = isPressed;
                joy2.setButton(normal2->second, normal2B || (turbo2B && turboPulse2));
            }
            else
                joy2.setButton(normal2->second, isPressed);
        }
        const auto turbo2 = player2TurboKeyMap.find(key);
        if (turbo2 != player2TurboKeyMap.end())
        {
            if (turbo2->second == R2NES::Core::IO::BUTTON_A)
                turbo2A = isPressed;
            if (turbo2->second == R2NES::Core::IO::BUTTON_B)
                turbo2B = isPressed;
            const bool normal = turbo2->second == R2NES::Core::IO::BUTTON_A ? normal2A : normal2B;
            joy2.setButton(turbo2->second, normal || (isPressed && turboPulse2));
        }
    }

    // Direciona o evento do gamepad ao controlador emulado do jogador correto.
    void InputManager::handleJoystick(int playerNum, SDL_GamepadButton button, bool isPressed, Core::NES &nes)
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
    void InputManager::handleJoystick1(SDL_GamepadButton button, bool isPressed, Core::NES &nes)
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
    void InputManager::handleJoystick2(SDL_GamepadButton button, bool isPressed, Core::NES &nes)
    {
        auto &joy2 = nes.getJoysticks().controller2;
        auto it = player2ControllerMap.find(button);
        if (it != player2ControllerMap.end())
        {
            joy2.setButton(it->second, isPressed);
        }
    }

    // Mantém o pulso de turbo ligado por um quadro a cada quatro quadros emulados.
    void InputManager::update(Core::NES &nes, uint64_t emulatedFrameCount)
    {
        auto &joy1 = nes.getJoysticks().controller1;
        const bool turboPulse = (emulatedFrameCount % 4 > 2);

        if (turboA)
            joy1.setButton(R2NES::Core::IO::BUTTON_A, turboPulse);
        if (turboB)
            joy1.setButton(R2NES::Core::IO::BUTTON_B, turboPulse);
        auto &joy2 = nes.getJoysticks().controller2;
        turboPulse2 = turboPulse;
        if (turbo2A)
            joy2.setButton(R2NES::Core::IO::BUTTON_A, normal2A || turboPulse);
        if (turbo2B)
            joy2.setButton(R2NES::Core::IO::BUTTON_B, normal2B || turboPulse);
    }
}