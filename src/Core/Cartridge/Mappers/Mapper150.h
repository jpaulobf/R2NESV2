#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <array>

namespace R2NES::Core
{
    class Mapper150 : public Mapper
    {
    public:
        Mapper150(uint16_t prg, uint16_t chr, MirrorMode mirror);
        static bool supports(uint16_t prg, uint16_t chr, MirrorMode mirror);
        bool cpuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped, uint8_t &data, uint32_t clock) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped, uint8_t data, uint32_t clock) override;
        MirrorMode getMirrorMode() override;
        void reset() override;
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        uint8_t index = 0;
        std::array<uint8_t, 8> registers{};
    };
}
