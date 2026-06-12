#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper004 : public Mapper
    {
    public:
        Mapper004(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper004();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;

        MirrorMode getMirrorMode() override;

        bool getIrqFlag() const override;
        void reset() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nTargetRegister = 0;
        bool bPRGBankMode = false;
        bool bCHRInversion = false;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;

        uint32_t pRegister[8] = {0};
        uint32_t pPRGMode[4] = {0};
        uint32_t pCHRMode[8] = {0};

        bool bIRQEnabled = false;
        bool bIRQActive = false;
        bool bIRQReload = false;
        uint16_t nIRQLatch = 0x00;
        uint16_t nIRQCounter = 0x00;
        uint16_t nLastA12 = 0x0000;

        uint8_t vPRGRAM[8192];

        void updateBanks();
    };
}