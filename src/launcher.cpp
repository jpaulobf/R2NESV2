#include <iostream>
#include <SDL.h>
#include "Window/Engine.h"

int main(int argc, char* argv[])
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;
    
    R2NES::Core::Engine emulator;
    emulator.run();
    
    return 0;
}