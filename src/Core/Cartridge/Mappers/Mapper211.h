#pragma once
#include "Core/Cartridge/Mappers/Mapper090.h"
#include "Core/Memory/VRAM/VRAM.h"

namespace R2NES::Core
{
    // J.Y. Company variant with programmable nametable sources.
    class Mapper211 : public Mapper090
    {
    public:
        Mapper211(uint16_t prg, uint16_t chr);
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapPeek(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        void onPpuAddress(uint16_t, uint32_t) override;
        VRAM *getCiram() override { return &ciram; }
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;

    protected:
        uint32_t chrAddress(uint16_t) const override;

    private:
        bool mapNametable(uint16_t, uint32_t &, uint8_t &) const;
        uint16_t ciramAddress(uint16_t) const;
        uint8_t ntLow[4]{}, ntHigh[4]{};
        uint8_t mirrorControl = 0, ntCompare = 0;
        uint8_t chrLatch[2] = {0, 4};
        VRAM ciram;
    };
}