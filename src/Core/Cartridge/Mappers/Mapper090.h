#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper090 : public Mapper
    {
    public:
        Mapper090(uint8_t prgBanks, uint8_t chrBanks);
        ~Mapper090();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        // Registradores de PRG (4 bancos de 8KB)
        uint8_t prgBanks[4] = {0, 0, 0, 0};

        // Registradores de CHR (8 bancos de 1KB)
        uint16_t chrBanks[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        uint8_t chrLow[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        uint8_t chrHigh[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        // Multiplicador de hardware
        uint8_t multA = 0;
        uint8_t multB = 0;

        // IRQ Control
        bool irqEnabled = false;
        uint8_t irqCounter = 0;
        uint8_t irqLatch = 0;
        uint8_t irqMode = 0; // 0: CPU cycles, 1: PPU (não implementado totalmente em mappers simples)

        // Mirroring
        uint8_t mirrorControl = 0;
    };
}