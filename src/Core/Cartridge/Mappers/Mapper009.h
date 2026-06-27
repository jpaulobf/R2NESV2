#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper009 : public Mapper
    {
    public:
        Mapper009(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper009();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        void reset() override;

        MirrorMode getMirrorMode() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        enum class LatchState : uint8_t
        {
            FD,
            FE
        };

        uint8_t nPRGBankSelect = 0;
        uint8_t nCHRBankSelect_0_FD = 0;
        uint8_t nCHRBankSelect_0_FE = 0;
        uint8_t nCHRBankSelect_1_FD = 0;
        uint8_t nCHRBankSelect_1_FE = 0;

        LatchState latch0 = LatchState::FE;
        LatchState latch1 = LatchState::FE;

        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
        MirrorMode ogMirrorMode = MirrorMode::HORIZONTAL;

        uint8_t vPRGRAM[8192] = {0};
    };
}