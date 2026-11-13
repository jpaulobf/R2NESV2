#include "System/InputManager.h"
#include "Core/NES.h"
#include <stdexcept>

using namespace R2NES;

static unsigned read(Core::IO::Controller &pad)
{
    pad.writeStrobe(1);
    pad.writeStrobe(0);
    unsigned value = 0;
    for (int i = 0; i < 8; ++i)
        value = (value << 1) | pad.readNextBit();
    return value;
}
static void check(bool condition)
{
    if (!condition)
        throw std::runtime_error("Keyboard mapping failed");
}
// Confere os padrões, o remapeamento e a combinação de turbo no segundo controle.
int main()
{
    Core::NES nes;
    System::InputManager input;
    input.handleKeyboard(SDLK_UP, true, nes);
    input.handleKeyboard(SDLK_Z, true, nes);
    input.handleKeyboard(SDLK_1, true, nes);
    check(read(nes.getJoysticks().controller2) == (Core::IO::BUTTON_UP | Core::IO::BUTTON_B | Core::IO::BUTTON_START));
    input.handleKeyboard(SDLK_UP, false, nes);
    input.handleKeyboard(SDLK_Z, false, nes);
    input.handleKeyboard(SDLK_1, false, nes);
    auto mapping = R2Keyboard::defaults();
    mapping[1].fill(0);
    mapping[0][0] = SDLK_Q;
    mapping[1][4] = SDLK_Z;
    mapping[1][7] = SDLK_X;
    mapping[1][5] = SDLK_C;
    input.setKeyboardMapping(mapping, nes);
    input.handleKeyboard(SDLK_W, true, nes);
    check(read(nes.getJoysticks().controller1) == 0);
    input.handleKeyboard(SDLK_Q, true, nes);
    input.handleKeyboard(SDLK_Z, true, nes);
    input.handleKeyboard(SDLK_X, true, nes);
    input.update(nes, 0);
    check(read(nes.getJoysticks().controller1) == Core::IO::BUTTON_UP);
    check(read(nes.getJoysticks().controller2) == Core::IO::BUTTON_B);
    input.update(nes, 3);
    check(read(nes.getJoysticks().controller2) == (Core::IO::BUTTON_B | Core::IO::BUTTON_A));
    input.handleKeyboard(SDLK_C, true, nes);
    input.update(nes, 0);
    check(read(nes.getJoysticks().controller2) == (Core::IO::BUTTON_B | Core::IO::BUTTON_A));
    input.handleKeyboard(SDLK_X, false, nes);
    check(read(nes.getJoysticks().controller2) == (Core::IO::BUTTON_B | Core::IO::BUTTON_A));
    input.setKeyboardMapping(R2Keyboard::defaults(), nes);
    check(read(nes.getJoysticks().controller1) == 0 && read(nes.getJoysticks().controller2) == 0);
}