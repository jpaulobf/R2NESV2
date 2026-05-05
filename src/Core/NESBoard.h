#pragma once
#include "Core/Bus/Bus.h"
#include "Core/CPU/CPU.h"
#include "Core/PPU/PPU.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include <string>

namespace R2NES::Core
{
    class NesBoard
    {
    public:
        NesBoard();
        ~NesBoard();

        void step();

        void insertCartridge(const std::string &path);

        void reset();

        void unload();

        CPU &getCpu() { return cpu; }

        PPU &getPpu() { return ppu; }

        // Permite ler um byte de qualquer lugar do barramento (debug)
        uint8_t cpuRead(uint16_t addr) { return bus.cpuRead(addr); }

        // Retorna o total de ciclos executados pelo sistema
        uint32_t getSystemClockCounter() const { return systemClockCounter; }

        bool isFrameComplete() const { return ppu.isFrameComplete(); }

        void clearFrameComplete() { ppu.clearFrameComplete(); }

        bool isCartridgeLoaded() const { return cartridgeLoaded; }

    private:
        Bus bus;
        RAM ram;
        CPU cpu;
        PPU ppu;

        // Controle de timing
        uint32_t systemClockCounter = 0;

        bool cartridgeLoaded = false;
    };
}