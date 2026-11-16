#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <array>

namespace R2NES::Core
{
    // Cony single-game PCBs: standard CHR (0) and large CHR (1).
    class Mapper083 : public Mapper
    {
    public:
        Mapper083(uint16_t prg, uint16_t chr, bool largeChr, MirrorMode initialMirror);
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override { return false; }
        void tick() override;
        void onPpuAddress(uint16_t, uint32_t) override;
        bool getIrqFlag() const override { return irqPending; }
        MirrorMode getMirrorMode() override;
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;

    private:
        void clockIrq();
        bool largeChr;
        MirrorMode initialMirror;
        uint8_t base = 0, mode = 0, source = 0;
        std::array<uint8_t, 4> prg{}, scratch{};
        std::array<uint8_t, 8> chr{};
        uint16_t counter = 0;
        bool irqEnabled = false, irqPending = false, a12 = false;
    };
}