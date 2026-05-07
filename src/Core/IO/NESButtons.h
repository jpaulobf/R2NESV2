#pragma once

namespace R2NES::Core::IO
{
    enum NESButtons {
        BUTTON_A      = 0x80, // 10000000
        BUTTON_B      = 0x40, // 01000000
        BUTTON_SELECT = 0x20, // 00100000
        BUTTON_START  = 0x10, // 00010000
        BUTTON_UP     = 0x08, // 00001000
        BUTTON_DOWN   = 0x04, // 00000100
        BUTTON_LEFT   = 0x02, // 00000010
        BUTTON_RIGHT  = 0x01  // 00000001
    };
}