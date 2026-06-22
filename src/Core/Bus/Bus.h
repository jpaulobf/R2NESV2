#pragma once
#include <cstdint>
#include <memory>
#include "Common/Common.h"
#include "Core/IO/Joysticks.h"
#include "Core/CPU/CPU.h"

namespace R2NES::Core
{
    class RAM;
    class Cartridge;
    class PPU;
    class APU;
    class CPU;

    namespace IO
    {
        class Joysticks;
    }

    class Bus
    {
    public:
        Bus();
        ~Bus();

        // Endereços dos Registradores da PPU (Memory Mapped I/O)
        enum PPU_REGISTERS : uint16_t
        {
            PPUCTRL = 0x2000,
            PPUMASK = 0x2001,
            PPUSTATUS = 0x2002,
            OAMADDR = 0x2003,
            OAMDATA = 0x2004,
            PPUSCROLL = 0x2005,
            PPUADDR = 0x2006,
            PPUDATA = 0x2007
        };

        // Conecta a CPU física ao barramento
        void connectCPU(CPU *pCpu);

        // Conecta a RAM física ao barramento
        void connectRam(RAM *pRam);

        // Conecta a APU ao barramento
        void connectAPU(APU *pApu);

        // Comunicação básica
        void cpuWrite(uint16_t addr, uint8_t data);
        uint8_t cpuRead(uint16_t addr, bool readOnly = false);

        // Comunicação do barramento de vídeo (PPU)
        bool ppuRead(uint16_t addr, uint8_t &data) const;
        bool ppuWrite(uint16_t addr, uint8_t data);
        MirrorMode getMirrorMode() const;

        // Conecta o cartucho inserido
        void setCartridge(const std::shared_ptr<Cartridge> &cartridge);

        // Conecta os joysticks ao barramento
        void connectJoysticks(IO::Joysticks *joysticks);

        // Conecta a PPU ao barramento
        void connectPPU(PPU *pPpu);

        // Verifica se a Pistola Zapper está sendo disparada
        void setZapperTrigger(bool pulled);

        void ppuAddressUpdated(uint16_t addr);

    public:
        RAM *ram = nullptr;
        PPU *ppu = nullptr;
        APU *apu = nullptr;
        CPU *cpu = nullptr;
        IO::Joysticks *joysticks = nullptr;
        std::shared_ptr<Cartridge> cart;
        uint32_t systemClockCounter = 0;

        uint8_t dma_page = 0x00;
        uint8_t dma_addr = 0x00;
        uint8_t dma_data = 0x00;
        bool dma_transfer = false;
        bool dma_dummy = true;

    private:
        bool zapperTrigger = false;
    };
}