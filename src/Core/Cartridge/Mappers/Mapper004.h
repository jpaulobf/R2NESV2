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
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                         uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data,
                        uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                         uint32_t systemClockCounter) override;
        void onPpuAddress(uint16_t addr, uint32_t systemClockCounter) override;
        void tick() override;

        MirrorMode getMirrorMode() override;

        bool getIrqFlag() const override;
        void clearIrqFlag() override;
        void reset() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nTargetRegister = 0;
        bool bPRGBankMode = false;
        bool bCHRInversion = false;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
        MirrorMode ogMirrorMode = MirrorMode::HORIZONTAL;

        uint8_t pRegister[8] = {0};
        uint32_t pPRGMode[4] = {0};
        uint32_t pCHRMode[8] = {0};

        bool bIRQEnabled = false;
        bool bIRQActive = false;
        bool bIRQReload = false;
        uint16_t nIRQLatch = 0x00;
        uint16_t nIRQCounter = 0x00;
        bool bPRGRAMEnabled = true;
        bool bPRGRAMWriteProtected = false;
        // 0xFFFF marks the first unobserved PPU address; otherwise 0 or 0x1000.
        uint16_t nLastA12 = 0xFFFF;

        uint8_t vPRGRAM[8192];

        // Saturating count of M2 edges observed while A12 remains low.
        uint8_t nA12LowM2Edges = 0;

        void updateBanks();
        void handleA12Edge(uint16_t addr);
    };
} // namespace R2NES::Core
