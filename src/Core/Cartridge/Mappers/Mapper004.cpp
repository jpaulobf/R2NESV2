#include "Core/Cartridge/Mappers/Mapper004.h"

namespace R2NES::Core
{
    Mapper004::Mapper004(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
    {
        mirrorMode = mirror;
        for (int i = 0; i < 8192; i++)
            vPRGRAM[i] = 0x00;
        reset();
    }

    Mapper004::~Mapper004() {}

    void Mapper004::reset()
    {
        nTargetRegister = 0;
        bPRGBankMode = false;
        bCHRInversion = false;
        mirrorMode = MirrorMode::HORIZONTAL;

        bIRQEnabled = false;
        bIRQActive = false;
        bIRQReload = false;
        nIRQLatch = 0x00;
        nIRQCounter = 0x00;
        nLastA12 = 0x0000;
        nLastA12Clock = 0; // Inicializado com segurança

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

    bool Mapper004::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            data = vPRGRAM[addr & 0x1FFF];
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint16_t offset = addr & 0x1FFF;
            uint8_t bank = (addr - 0x8000) / 0x2000;
            mapped_addr = pPRGMode[bank] * 0x2000 + offset;
            return true;
        }

        return false;
    }

    bool Mapper004::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
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
                mirrorMode = (data & 0x01) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;
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

    // Método auxiliar interno para centralizar o comportamento do contador de IRQ
    void Mapper004::handleA12Edge(uint16_t addr, uint32_t systemClockCounter)
    {
        uint16_t currentA12 = addr & 0x1000;

        // Se o clock do sistema avançou consideravelmente desde o último acesso,
        // significa que mudamos de ciclo de instrução/pixel. Forçamos o decaimento
        // da linha A12 para simular o período em que a PPU ficou ociosa ou lendo Nametables.
        if ((systemClockCounter - nLastA12Clock) > 100)
        {
            nLastA12 = 0;
        }

        if (nLastA12 == 0 && currentA12 != 0)
        {
            if ((systemClockCounter - nLastA12Clock) > 15)
            {
                if (nIRQCounter == 0 || bIRQReload)
                {
                    nIRQCounter = nIRQLatch;
                }
                else
                {
                    nIRQCounter--;
                }

                if (nIRQCounter == 0 && bIRQEnabled)
                    bIRQActive = true;

                bIRQReload = false;
            }
            nLastA12Clock = systemClockCounter;
        }

        nLastA12 = currentA12;
    }

    bool Mapper004::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        handleA12Edge(addr, systemClockCounter);

        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            return true;
        }
        return false;
    }

    // Atualizado para receber e processar o relógio do sistema também em escritas
    bool Mapper004::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        handleA12Edge(addr, systemClockCounter);

        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            return true;
        }
        return false;
    }

    MirrorMode Mapper004::getMirrorMode()
    {
        return mirrorMode;
    }

    bool Mapper004::getIrqFlag() const
    {
        return bIRQActive;
    }

    void Mapper004::updateBanks()
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

    void Mapper004::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nTargetRegister), sizeof(nTargetRegister));
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
        os.write(reinterpret_cast<const char *>(&nLastA12Clock), sizeof(nLastA12Clock));
        os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
    }

    void Mapper004::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nTargetRegister), sizeof(nTargetRegister));
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
        is.read(reinterpret_cast<char *>(&nLastA12Clock), sizeof(nLastA12Clock));
        is.read(reinterpret_cast<char *>(vPRGRAM), sizeof(vPRGRAM));
        updateBanks();
    }
}