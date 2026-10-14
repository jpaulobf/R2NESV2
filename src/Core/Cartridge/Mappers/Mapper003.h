#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>
#include <vector>

namespace R2NES::Core
{
    class Mapper003 : public Mapper
    {
    public:
        Mapper003(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, uint32_t prgRamSize = 0);
        ~Mapper003();
        static bool supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, uint32_t prgRamSize = 0);
        void reset() override;

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;

        MirrorMode getMirrorMode() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nCHRBankSelect = 0;
        uint8_t latchMask = 0x03;
        std::vector<uint8_t> prgRAM;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
    };
}
