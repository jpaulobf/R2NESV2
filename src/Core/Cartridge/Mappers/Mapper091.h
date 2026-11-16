#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <array>

namespace R2NES::Core
{
    // HK-SF3 / J.Y. board, NES 2.0 submapper 0.
    class Mapper091 : public Mapper
    {
    public:
        Mapper091(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override { return false; }
        void onPpuAddress(uint16_t, uint32_t) override;
        bool getIrqFlag() const override { return irqPending; }
        MirrorMode getMirrorMode() override { return mirror; }
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;

    private:
        MirrorMode mirror;
        std::array<uint8_t, 2> prg{};
        std::array<uint8_t, 4> chr{};
        uint8_t outer = 0, irqCounter = 0;
        bool irqEnabled = false, irqPending = false, lastA12 = false;
    };
}