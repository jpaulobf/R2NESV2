#pragma once

#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper202 : public Mapper
    {
    public:
        Mapper202(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
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
        MirrorMode initialMirrorMode;
        // 0xFF representa o estado inicial, antes da primeira escrita.
        uint8_t addressLatch = 0xFF;
    };
}