#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper002 : public Mapper
    {
    public:
        Mapper002(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror);
        ~Mapper002();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;

        MirrorMode getMirrorMode() override;

    private:
        uint8_t nPRGBankSelect = 0;
        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
    };
}