#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>
#include <array>

namespace R2NES::Core
{
    class Mapper009 : public Mapper
    {
    public:
        struct Configuration
        {
            // Extensão de placa com RAM externa declarada no header; PxROM normal não a tem.
            uint32_t prgRamSize = 0;
            uint32_t prgNvRamSize = 0;
        };
        Mapper009(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        Mapper009(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, Configuration config);
        ~Mapper009();
        static bool supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        static bool supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, Configuration config);
        std::vector<uint8_t> getBatteryData() const override;
        bool setBatteryData(const std::vector<uint8_t> &data) override;

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        // Inicialização completa e determinística do emulador, não reset elétrico.
        void reset() override;

        MirrorMode getMirrorMode() override;

        // Serialização para Save / Load states
        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

    private:
        Configuration configuration;
        std::array<uint8_t, 8192> prgRam{};
        enum class LatchState : uint8_t
        {
            FD,
            FE
        };

        uint8_t nPRGBankSelect = 0;
        uint8_t nCHRBankSelect_0_FD = 0;
        uint8_t nCHRBankSelect_0_FE = 0;
        uint8_t nCHRBankSelect_1_FD = 0;
        uint8_t nCHRBankSelect_1_FE = 0;

        LatchState latch0 = LatchState::FE;
        LatchState latch1 = LatchState::FE;

        MirrorMode mirrorMode = MirrorMode::HORIZONTAL;
        MirrorMode ogMirrorMode = MirrorMode::HORIZONTAL;
    };
}
