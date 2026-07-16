#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper011 : public Mapper
    {
    public:
        Mapper011(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper011();

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
        uint8_t nPRGBankSelect = 0x00;
        uint8_t nCHRBankSelect = 0x00;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
    };
}