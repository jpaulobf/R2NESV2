#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <iostream>
#include <istream>
#include <array>

namespace R2NES::Core
{
    class Mapper004 : public Mapper
    {
    public:
        struct Configuration
        {
            // NES 2.0: 0 = Sharp, 1 = MMC6, 2 = mirroring fixo,
            // 3 = MC-ACC, 4 = NEC, 5 = T9552. A configuração da placa
            // permanece fixa; os registradores abaixo são o estado mutável.
            uint8_t submapper = 0;
            uint32_t prgRamSize = 8192;
            uint32_t prgNvRamSize = 0;
            bool extendedChr = false; // Somente derivados como Mapper 187.
        };
        Mapper004(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror);
        Mapper004(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, const Configuration &config);
        static bool supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, const Configuration &config);
        void loadTrainer(const uint8_t *data);
        ~Mapper004();
        std::vector<uint8_t> getBatteryData() const override;
        bool setBatteryData(const std::vector<uint8_t> &data) override;

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
        Configuration configuration;
        bool mmc6RamEnabled = false;
        uint8_t mmc6RamPermissions = 0;
        uint8_t mcAccPrescaler = 0;
        uint8_t scramblingPattern = 0;
        std::array<uint8_t, 2048> extraNametables{};
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
        // 0xFFFF indica que ainda não observamos A12; depois, vale 0 ou 0x1000.
        uint16_t nLastA12 = 0xFFFF;

        uint8_t vPRGRAM[8192];

        // Contagem saturada das bordas M2 observadas enquanto A12 fica baixo.
        uint8_t nA12LowM2Edges = 0;

        void updateBanks();
        void handleA12Edge(uint16_t addr);
        void clockIrqCounter();
        uint32_t scrambleAddress(uint32_t address, bool chr) const;
    };
}
