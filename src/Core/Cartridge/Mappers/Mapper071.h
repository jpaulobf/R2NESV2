#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper071 : public Mapper
    {
    public:
        Mapper071(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror,
                  uint8_t submapper = 0, bool legacyMirroringDetection = true);

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;

        MirrorMode getMirrorMode() override;
        void reset() override;
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nPRGBankSelect = 0;
        const bool singleScreenMirroring;
        const bool autoDetectMirroring;
        const MirrorMode initialMirrorMode;
        MirrorMode mirrorMode;
    };
}