#pragma once
#include "Core/Cartridge/Mappers/Mapper004.h"

namespace R2NES::Core
{
    // Lógica de CHR/IRQ do MMC3 com seleção independente de bancos PRG de 32 KiB.
    class Mapper189 : public Mapper004
    {
    public:
        Mapper189(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data,
                         uint32_t systemClockCounter) override;
        void reset() override;
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t prgBank = 0;
    };
}