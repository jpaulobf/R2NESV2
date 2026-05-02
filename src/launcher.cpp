#include <iostream>
#include "Core/NESBoard.h"
#include "Util/NESTest.h"

int main()
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;

    R2NES::Core::NesBoard nes;
    R2NES::Core::NESTest tester(nes);

    if (!tester.run("..\\nestest\\nestest.nes", 8991))
    {
        std::cerr << "NESTest failed or log could not be created." << std::endl;
        return -1;
    }

    return 0;
}