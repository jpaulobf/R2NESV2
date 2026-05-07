#pragma once

#include "Controller.h"

namespace R2NES::Core::IO
{
    class Joysticks
    {
        public:
            Controller controller1;
            Controller controller2;

        public:
            Joysticks() {};
            ~Joysticks() {};
    };
}