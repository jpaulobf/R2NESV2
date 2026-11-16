#pragma once
#include "Core/Cartridge/Mappers/Mapper004.h"

namespace R2NES::Core
{
    class Mapper052 : public Mapper004
    {
    public:
        Mapper052(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, uint32_t ramSize, bool battery);
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;

    private:
        uint16_t totalPrg, totalChr;
        uint8_t outer = 0;
    };
}