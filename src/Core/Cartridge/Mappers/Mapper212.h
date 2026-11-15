#pragma once

#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper212 : public Mapper
    {
    public:
        Mapper212(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        static bool supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);

        bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
        MirrorMode getMirrorMode() override;
        void reset() override;
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        // A0-A3 e A14 da última escrita, compactados em cinco bits.
        uint8_t addressLatch = 0;
    };
}
