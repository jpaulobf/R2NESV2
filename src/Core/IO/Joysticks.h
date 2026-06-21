#pragma once

#include "Controller.h"

namespace R2NES::Core::IO
{
    enum class DeviceType
    {
        Gamepad,
        Zapper,
        None
    };

    class Joysticks
    {
    public:
        Controller controller1;
        Controller controller2;
        DeviceType port2Device = DeviceType::Gamepad;

    public:
        Joysticks() {};
        ~Joysticks() {};
    };
}