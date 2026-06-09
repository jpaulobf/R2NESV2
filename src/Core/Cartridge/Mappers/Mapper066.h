#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper066 : public Mapper
    {
    public:
        Mapper066(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper066();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

        void reset() override;

        MirrorMode getMirrorMode() override;

    private:
        uint8_t nPRGBankSelect = 0x00;
        uint8_t nCHRBankSelect = 0x00;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
    };
}