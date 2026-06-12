#pragma once

namespace R2NES::Core
{
    enum class MirrorMode
    {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
        FOUR_SCREEN
    };

    struct MouseState
    {
        int x = -1;
        int y = -1;
        bool leftButton = false;
    };

    enum class DisplayMode
    {
        WINDOWED,
        FULLSCREEN_STRETCH,
        FULLSCREEN_ASPECT_8_7
    };

    // Tipos de paletas disponíveis
    enum class PaletteType
    {
        DEFAULT,
        SMOOTH,
        NESTOPIA,
        WAVEBEAM,
        NEON
    };
}