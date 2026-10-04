#include "Core/Cartridge/Mappers/Mapper187.h"

namespace R2NES::Core
{
    Mapper187::Mapper187(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
    {
        mirrorMode = mirror;
        ogMirrorMode = mirror; // Armazena o modo de espelhamento original para reset
        for (int i = 0; i < 8192; i++)
            vPRGRAM[i] = 0x00;
        reset();
    }

    Mapper187::~Mapper187() {}

    void Mapper187::reset()
    {
        nromModeRegister = 0;
        nTargetRegister = 0;
        bPRGBankMode = false;
        bCHRInversion = false;
        mirrorMode = ogMirrorMode;

        bIRQEnabled = false;
        bIRQActive = false;
        bIRQReload = false;
        nIRQLatch = 0x00;
        nIRQCounter = 0x00;
        nLastA12 = 0xFFFF; // The first PPU address establishes the initial A12 level.
        nA12LowStartClock = 0;

        // Inicialização padrão segura do MMC3
        pRegister[0] = 0;
        pRegister[1] = 2;
        pRegister[2] = 4;
        pRegister[3] = 5;
        pRegister[4] = 6;
        pRegister[5] = 7;
        pRegister[6] = 0;
        pRegister[7] = 1;

        updateBanks();
    }

    bool Mapper187::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            data = vPRGRAM[addr & 0x1FFF];
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // $5000 bit 7 selects the NROM override. The low bank bits choose
            // a 16 KiB bank; bit 5 selects NROM-256 behavior by routing CPU A14
            // to PRG A14. With bit 5 clear, the selected 16 KiB bank is mirrored.
            if (nromModeRegister & 0x80)
            {
                const uint32_t total16kBanks = nPRGBanks;
                if (total16kBanks == 0)
                    return false;

                uint32_t bank16 = nromModeRegister & 0x0F;
                const uint32_t cpuA14 = (addr >> 14) & 0x01;
                if (nromModeRegister & 0x20)
                    bank16 = (bank16 & ~1u) | cpuA14;
                bank16 %= total16kBanks;
                mapped_addr = bank16 * 0x4000 + (addr & 0x3FFF);
                return true;
            }

            uint16_t offset = addr & 0x1FFF;
            uint8_t bank = (addr - 0x8000) / 0x2000;
            mapped_addr = pPRGMode[bank] * 0x2000 + offset;
            return true;
        }

        return false;
    }

    bool Mapper187::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        // The NROM override register is decoded at $5000 (low address bits
        // are not connected on the board).
        if ((addr & 0xF007) == 0x5000)
        {
            nromModeRegister = data;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }

        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            vPRGRAM[addr & 0x1FFF] = data;
            return true;
        }

        if (addr >= 0x8000 && addr <= 0x9FFF)
        {
            if (!(addr & 0x0001))
            {
                nTargetRegister = data & 0x07;
                bPRGBankMode = (data & 0x40);
                bCHRInversion = (data & 0x80);
                updateBanks();
            }
            else
            {
                if (nTargetRegister <= 1)
                {
                    pRegister[nTargetRegister] = data & 0xFE;
                }
                else
                {
                    pRegister[nTargetRegister] = data;
                }
                updateBanks();
            }
            return false;
        }

        if (addr >= 0xA000 && addr <= 0xBFFF)
        {
            if (!(addr & 0x0001))
            {
                mirrorMode = (data & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
            }
            return false;
        }

        if (addr >= 0xC000 && addr <= 0xDFFF)
        {
            if (!(addr & 0x0001))
                nIRQLatch = data;
            else
                bIRQReload = true;

            return false;
        }

        if (addr >= 0xE000 && addr <= 0xFFFF)
        {
            if (!(addr & 0x0001))
            {
                bIRQEnabled = false;
                bIRQActive = false;
            }
            else
                bIRQEnabled = true;
            return false;
        }

        return false;
    }

    // Centralizes the MMC3 filtered-A12 IRQ clock.
    void Mapper187::handleA12Edge(uint16_t addr, uint32_t systemClockCounter)
    {
        constexpr uint16_t A12_UNINITIALIZED = 0xFFFF;
        constexpr uint16_t A12_LOW = 0x0000;
        constexpr uint16_t A12_HIGH = 0x1000;
        // The MMC3 filter requires A12 to remain low across three falling M2 edges.
        constexpr uint32_t A12_LOW_FILTER_M2_CYCLES = 3;

        const uint16_t currentA12 = addr & A12_HIGH;

        if (nLastA12 == A12_UNINITIALIZED)
        {
            nLastA12 = currentA12;
            if (currentA12 == A12_LOW)
                nA12LowStartClock = systemClockCounter;
            return;
        }

        if (currentA12 == nLastA12)
            return;

        if (currentA12 == A12_LOW)
        {
            nA12LowStartClock = systemClockCounter;
            nLastA12 = A12_LOW;
            return;
        }

        const uint32_t lowDuration = systemClockCounter - nA12LowStartClock;
        nLastA12 = A12_HIGH;
        if (lowDuration < A12_LOW_FILTER_M2_CYCLES)
            return;

        if (nIRQCounter == 0 || bIRQReload)
        {
            nIRQCounter = nIRQLatch;
        }
        else
        {
            nIRQCounter--;
            if (nIRQCounter == 0 && bIRQEnabled)
                bIRQActive = true;
        }

        bIRQReload = false;
    }

    bool Mapper187::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        handleA12Edge(addr, systemClockCounter);

        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            if (nCHRBanks > 32)
                mapped_addr = (mapped_addr & 0x3FFFF) | ((addr & 0x1000) ? 0x40000 : 0);
            return true;
        }
        return false;
    }

    // Atualizado para receber e processar o relógio do sistema também em escritas
    bool Mapper187::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        handleA12Edge(addr, systemClockCounter);

        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            if (nCHRBanks > 32)
                mapped_addr = (mapped_addr & 0x3FFFF) | ((addr & 0x1000) ? 0x40000 : 0);
            return true;
        }
        return false;
    }

    MirrorMode Mapper187::getMirrorMode()
    {
        return mirrorMode;
    }

    bool Mapper187::getIrqFlag() const
    {
        return bIRQActive;
    }

    void Mapper187::clearIrqFlag()
    {
        bIRQActive = false;
    }

    void Mapper187::updateBanks()
    {
        uint32_t nPRG8 = nPRGBanks * 2;
        uint32_t nCHR1 = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (bPRGBankMode)
        {
            pPRGMode[0] = (nPRG8 - 2) % nPRG8;
            pPRGMode[1] = pRegister[7] % nPRG8;
            pPRGMode[2] = pRegister[6] % nPRG8;
            pPRGMode[3] = (nPRG8 - 1) % nPRG8;
        }
        else
        {
            pPRGMode[0] = pRegister[6] % nPRG8;
            pPRGMode[1] = pRegister[7] % nPRG8;
            pPRGMode[2] = (nPRG8 - 2) % nPRG8;
            pPRGMode[3] = (nPRG8 - 1) % nPRG8;
        }

        if (bCHRInversion)
        {
            pCHRMode[0] = pRegister[2] % nCHR1;
            pCHRMode[1] = pRegister[3] % nCHR1;
            pCHRMode[2] = pRegister[4] % nCHR1;
            pCHRMode[3] = pRegister[5] % nCHR1;
            pCHRMode[4] = (pRegister[0] & 0xFE) % nCHR1;
            pCHRMode[5] = ((pRegister[0] & 0xFE) + 1) % nCHR1;
            pCHRMode[6] = (pRegister[1] & 0xFE) % nCHR1;
            pCHRMode[7] = ((pRegister[1] & 0xFE) + 1) % nCHR1;
        }
        else
        {
            pCHRMode[0] = (pRegister[0] & 0xFE) % nCHR1;
            pCHRMode[1] = ((pRegister[0] & 0xFE) + 1) % nCHR1;
            pCHRMode[2] = (pRegister[1] & 0xFE) % nCHR1;
            pCHRMode[3] = ((pRegister[1] & 0xFE) + 1) % nCHR1;
            pCHRMode[4] = pRegister[2] % nCHR1;
            pCHRMode[5] = pRegister[3] % nCHR1;
            pCHRMode[6] = pRegister[4] % nCHR1;
            pCHRMode[7] = pRegister[5] % nCHR1;
        }
    }

    void Mapper187::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nTargetRegister), sizeof(nTargetRegister));
        os.write(reinterpret_cast<const char *>(&nromModeRegister), sizeof(nromModeRegister));
        os.write(reinterpret_cast<const char *>(&bPRGBankMode), sizeof(bPRGBankMode));
        os.write(reinterpret_cast<const char *>(&bCHRInversion), sizeof(bCHRInversion));
        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
        os.write(reinterpret_cast<const char *>(pRegister), sizeof(pRegister));
        os.write(reinterpret_cast<const char *>(&bIRQEnabled), sizeof(bIRQEnabled));
        os.write(reinterpret_cast<const char *>(&bIRQActive), sizeof(bIRQActive));
        os.write(reinterpret_cast<const char *>(&bIRQReload), sizeof(bIRQReload));
        os.write(reinterpret_cast<const char *>(&nIRQLatch), sizeof(nIRQLatch));
        os.write(reinterpret_cast<const char *>(&nIRQCounter), sizeof(nIRQCounter));
        os.write(reinterpret_cast<const char *>(&nLastA12), sizeof(nLastA12));
        os.write(reinterpret_cast<const char *>(&nA12LowStartClock), sizeof(nA12LowStartClock));
        os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
    }

    void Mapper187::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nTargetRegister), sizeof(nTargetRegister));
        is.read(reinterpret_cast<char *>(&nromModeRegister), sizeof(nromModeRegister));
        is.read(reinterpret_cast<char *>(&bPRGBankMode), sizeof(bPRGBankMode));
        is.read(reinterpret_cast<char *>(&bCHRInversion), sizeof(bCHRInversion));
        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
        is.read(reinterpret_cast<char *>(pRegister), sizeof(pRegister));
        is.read(reinterpret_cast<char *>(&bIRQEnabled), sizeof(bIRQEnabled));
        is.read(reinterpret_cast<char *>(&bIRQActive), sizeof(bIRQActive));
        is.read(reinterpret_cast<char *>(&bIRQReload), sizeof(bIRQReload));
        is.read(reinterpret_cast<char *>(&nIRQLatch), sizeof(nIRQLatch));
        is.read(reinterpret_cast<char *>(&nIRQCounter), sizeof(nIRQCounter));
        is.read(reinterpret_cast<char *>(&nLastA12), sizeof(nLastA12));
        is.read(reinterpret_cast<char *>(&nA12LowStartClock), sizeof(nA12LowStartClock));
        is.read(reinterpret_cast<char *>(vPRGRAM), sizeof(vPRGRAM));
        updateBanks();
    }
}
