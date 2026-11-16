#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <array>

namespace R2NES::Core
{
    class Mapper210 : public Mapper
    {
    public:
        Mapper210(uint16_t prg, uint16_t chr, MirrorMode mirror, bool namco175, size_t ramSize, bool battery);
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;
        MirrorMode getMirrorMode() override { return mirror; }
        void reset() override;
        void saveState(std::ostream &) override;
        void loadState(std::istream &) override;
        std::vector<uint8_t> getBatteryData() const override;
        bool setBatteryData(const std::vector<uint8_t> &) override;

    private:
        bool namco175, battery, writeEnabled = false;
        MirrorMode initialMirror, mirror;
        std::vector<uint8_t> ram;
        std::array<uint8_t, 3> prg{};
        std::array<uint8_t, 8> chr{};
    };
}