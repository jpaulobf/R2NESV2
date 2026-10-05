#pragma once
#include "Core/Cartridge/Mappers/Mapper004.h"

namespace R2NES::Core
{
    class Mapper187 : public Mapper004
    {
    public:
        Mapper187(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                         uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data,
                        uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                         uint32_t systemClockCounter) override;

        void reset() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nromModeRegister = 0;
    };
} // namespace R2NES::Core
