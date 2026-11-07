#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper090 : public Mapper
    {
    public:
        Mapper090(uint16_t prgBanks, uint16_t chrBanks);
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        void onPpuAddress(uint16_t, uint32_t) override;
        void onPpuRead(uint16_t) override;
        void tick() override;
        bool getIrqFlag() const override { return irqPending; }
        MirrorMode getMirrorMode() override;
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;

    private:
        void clockIrq();
        uint32_t chrAddress(uint16_t) const;
        uint8_t prg[4]{}, chrLow[8]{}, chrHigh[8]{};
        uint8_t mode = 0, mirror = 0, block = 0x20;
        uint8_t multA = 0, multB = 0, scratch = 0;
        uint8_t irqMode = 0, irqPrescaler = 0, irqCounter = 0, irqXor = 0, irqExtra = 0;
        bool irqEnabled = false, irqPending = false;
        uint16_t lastPpuAddress = 0;
    };
}