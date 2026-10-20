#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "System/Engine.h"

int main(int argc, char *argv[])
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;

    R2NES::Core::Engine emulator;
    emulator.run();

    return 0;
}