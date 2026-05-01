#pragma once
#include <cstdint>
#include <memory>

namespace R2NES::Core {
    class RAM;
    class Cartridge;

    class Bus {
    public:
        Bus();
        ~Bus();

        // Endereços dos Registradores da PPU (Memory Mapped I/O)
        enum PPU_REGISTERS : uint16_t {
            PPUCTRL   = 0x2000,
            PPUMASK   = 0x2001,
            PPUSTATUS = 0x2002,
            OAMADDR   = 0x2003,
            OAMDATA   = 0x2004,
            PPUSCROLL = 0x2005,
            PPUADDR   = 0x2006,
            PPUDATA   = 0x2007
        };

        // Conecta a RAM física ao barramento
        void connectRam(RAM* pRam);

        // Comunicação básica
        void cpuWrite(uint16_t addr, uint8_t data);
        uint8_t cpuRead(uint16_t addr, bool readOnly = false);

        // Conecta o cartucho inserido
        void setCartridge(const std::shared_ptr<Cartridge>& cartridge);

    public:
        RAM* ram = nullptr;
        std::shared_ptr<Cartridge> cart;
    };
}