#pragma once
#include "Core/Bus/Bus.h"
#include "Core/CPU/CPU.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h" // Adicionado para incluir o Cartridge
#include <string>

namespace R2NES::Core
{
    class NesBoard
    {
    public:
        NesBoard();

        void step();

        void insertCartridge(const std::string &path);

        void reset();

    private:
        Bus bus;
        RAM ram;
        CPU cpu;
        // Ppu ppu; // Será adicionado mais tarde

        // Controle de timing
        uint32_t systemClockCounter = 0;
    };
}