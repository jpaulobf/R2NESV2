#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>
#include <vector>

namespace R2NES::Core
{
    class Mapper023 : public Mapper
    {
    public:
        Mapper023(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper023();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;

        MirrorMode getMirrorMode() override;
        bool getIrqFlag() const override;
        void clearIrqFlag() override;
        void tick() override;
        void reset() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
        MirrorMode ogMirrorMode = MirrorMode::HORIZONTAL;

        // Regs de bancos PRG (unidades de 8KB)
        uint8_t prgBank0 = 0;
        uint8_t prgBank1 = 1;
        bool prgMode = false;

        // PRG RAM de 8KB ($6000-$7FFF)
        uint8_t vPRGRAM[8192] = {0};
        bool prgRamEnable = true;

        // Regs de bancos CHR (8 bancos de 1KB)
        uint16_t chrBank[8] = {0};
        uint8_t chrBankRegLow[8] = {0};
        uint8_t chrBankRegHigh[8] = {0};

        // IRQ VRC4
        uint8_t irqLatch = 0;
        uint8_t irqCounter = 0;
        bool irqEnabled = false;
        bool irqEnableOnAck = false;
        bool irqMode = false; // false = prescaler mode, true = cpu mode
        bool irqActive = false;
        int16_t irqPrescaler = 341;

        void updateChrBank(uint8_t index);
    };
}