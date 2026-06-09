#pragma once
#include "Core/Bus/Bus.h"
#include "Core/CPU/CPU.h"
#include "Core/PPU/PPU.h"
#include "Core/APU/APU.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include <string>
#include "Core/IO/Joysticks.h"

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

        Bus &getBus() { return bus; }

        APU &getApu() { return apu; }

        // Permite ler um byte de qualquer lugar do barramento (debug)
        uint8_t cpuRead(uint16_t addr) { return bus.cpuRead(addr); }

        // Retorna o total de ciclos executados pelo sistema
        uint32_t getSystemClockCounter() const { return bus.systemClockCounter; }

        bool isFrameComplete() const { return ppu.isFrameComplete(); }

        void clearFrameComplete() { ppu.clearFrameComplete(); }

        bool isCartridgeLoaded() const { return cartridgeLoaded; }

        IO::Joysticks &getJoysticks() { return joysticks; }

        bool saveState(const std::string &filename);

        bool loadState(const std::string &filename);

    private:
        Bus bus;
        RAM ram;
        CPU cpu;
        PPU ppu;
        APU apu;
        IO::Joysticks joysticks;

        bool cartridgeLoaded = false;
    };
}