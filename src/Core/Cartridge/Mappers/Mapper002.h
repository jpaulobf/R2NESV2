#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper002 : public Mapper
    {
    public:
        Mapper002(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        ~Mapper002();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;

        MirrorMode getMirrorMode() override;

        // Reinicialização completa usada por NES::reset(), não o reset elétrico do console.
        void reset() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t nPRGBankSelect = 0;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
    };
}