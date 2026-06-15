#pragma once

#include <cstdint>
#include "NESButtons.h"

namespace R2NES::Core::IO
{
    class Controller
    {
    private:
        uint8_t state = 0;
        uint8_t shift = 0;
        bool strobe = false;

    public:
        Controller() {};
        ~Controller() {};

        void writeStrobe(uint8_t data)
        {
            strobe = (data & 0x01);
            if (strobe)
            {
                shift = state;
            }
        }

        uint8_t readNextBit()
        {
            if (strobe)
            {
                shift = state;
                return (state & 0x80) > 0;
            }

            uint8_t data = (shift & 0x80) > 0;
            shift <<= 1;
            shift |= 0x01;

            return data;
        }

        void setButton(NESButtons button, bool pressed)
        {
            if (pressed)
                state |= button;
            else
                state &= ~button;
        }
    };
}