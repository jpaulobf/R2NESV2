#include "Core/Cartridge/Mappers/Mapper023.h"
#include <cstring>

namespace R2NES::Core
{
    Mapper023::Mapper023(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror)
        : Mapper(prgBanks, chrBanks), mirrorMode(mirror), ogMirrorMode(mirror)
    {
        reset();
    }

    Mapper023::~Mapper023() {}

    void Mapper023::reset()
    {
        mirrorMode = ogMirrorMode;
        prgBank0 = 0;
        prgBank1 = 1;
        prgMode = false;
        prgRamEnable = true;

        std::memset(vPRGRAM, 0, sizeof(vPRGRAM));
        std::memset(chrBank, 0, sizeof(chrBank));
        std::memset(chrBankRegLow, 0, sizeof(chrBankRegLow));
        std::memset(chrBankRegHigh, 0, sizeof(chrBankRegHigh));

        for (int i = 0; i < 8; i++)
        {
            chrBank[i] = i;
        }

        irqLatch = 0;
        irqCounter = 0;
        irqEnabled = false;
        irqEnableOnAck = false;
        irqMode = false;
        irqActive = false;
        irqPrescaler = 341;
    }

    void Mapper023::updateChrBank(uint8_t index)
    {
        if (index < 8)
        {
            chrBank[index] = (chrBankRegHigh[index] << 4) | (chrBankRegLow[index] & 0x0F);
        }
    }

    bool Mapper023::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            if (prgRamEnable)
            {
                mapped_addr = 0xFFFFFFFF;
                data = vPRGRAM[addr & 0x1FFF];
                return true;
            }
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint32_t total8kBanks = nPRGBanks * 2;
            if (total8kBanks == 0)
                return false;

            uint32_t bank8 = prgMode ? (total8kBanks - 2) : prgBank0;
            uint32_t bankA = prgBank1;
            uint32_t bankC = prgMode ? prgBank0 : (total8kBanks - 2);
            uint32_t bankE = total8kBanks - 1;

            uint32_t targetBank = 0;
            uint32_t offset = addr & 0x1FFF;

            if (addr <= 0x9FFF)
                targetBank = bank8;
            else if (addr <= 0xBFFF)
                targetBank = bankA;
            else if (addr <= 0xDFFF)
                targetBank = bankC;
            else
                targetBank = bankE;

            targetBank %= total8kBanks;
            mapped_addr = targetBank * 8192 + offset;
            return true;
        }

        return false;
    }

    bool Mapper023::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            if (prgRamEnable)
            {
                vPRGRAM[addr & 0x1FFF] = data;
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            mapped_addr = 0xFFFFFFFF;

            uint16_t major = addr & 0xF000;

            // Decodificação flexível para VRC2b (CPU A0, A1), VRC4e (CPU A2, A3) e VRC4f (CPU A3, A2)
            uint8_t regOffset = 0;
            regOffset |= (addr & 0x01);                       // A0 -> bit 0 (VRC2b)
            regOffset |= (addr & 0x02);                       // A1 -> bit 1 (VRC2b)
            regOffset |= ((addr >> 2) & 0x01);                // A2 -> bit 0 (VRC4e)
            regOffset |= ((addr >> 2) & 0x02);                // A3 -> bit 1 (VRC4e)
            regOffset |= ((addr >> 3) & 0x01);                // A3 -> bit 0 (VRC4f)
            regOffset |= ((addr >> 1) & 0x02);                // A2 -> bit 1 (VRC4f)

            switch (major)
            {
            case 0x8000: // PRG Select 0 ($8000-$8003)
                prgBank0 = data & 0x1F;
                break;

            case 0x9000: // Espelhamento & PRG Swap / WRAM Control ($9000-$9003)
                if (regOffset == 0 || regOffset == 1)
                {
                    switch (data & 0x03)
                    {
                    case 0:
                        mirrorMode = MirrorMode::VERTICAL;
                        break;
                    case 1:
                        mirrorMode = MirrorMode::HORIZONTAL;
                        break;
                    case 2:
                        mirrorMode = MirrorMode::ONESCREEN_LO;
                        break;
                    case 3:
                        mirrorMode = MirrorMode::ONESCREEN_HI;
                        break;
                    }
                }
                else if (regOffset == 2 || regOffset == 3)
                {
                    prgMode = (data & 0x02) != 0;
                    prgRamEnable = (data & 0x01) != 0;
                }
                break;

            case 0xA000: // PRG Select 1 ($A000-$A003)
                prgBank1 = data & 0x1F;
                break;

            case 0xB000: // CHR Select 0 & 1
            case 0xC000: // CHR Select 2 & 3
            case 0xD000: // CHR Select 4 & 5
            case 0xE000: // CHR Select 6 & 7
            {
                uint8_t baseBank = ((major - 0xB000) / 0x1000) * 2;
                if (regOffset == 0)
                {
                    chrBankRegLow[baseBank] = data & 0x0F;
                    updateChrBank(baseBank);
                }
                else if (regOffset == 1)
                {
                    chrBankRegHigh[baseBank] = data & 0x1F;
                    updateChrBank(baseBank);
                }
                else if (regOffset == 2)
                {
                    chrBankRegLow[baseBank + 1] = data & 0x0F;
                    updateChrBank(baseBank + 1);
                }
                else if (regOffset == 3)
                {
                    chrBankRegHigh[baseBank + 1] = data & 0x1F;
                    updateChrBank(baseBank + 1);
                }
                break;
            }

            case 0xF000: // IRQ Control ($F000-$F003)
                if (regOffset == 0)
                {
                    irqLatch = (irqLatch & 0xF0) | (data & 0x0F);
                }
                else if (regOffset == 1)
                {
                    irqLatch = (irqLatch & 0x0F) | ((data & 0x0F) << 4);
                }
                else if (regOffset == 2)
                {
                    irqEnableOnAck = (data & 0x01) != 0;
                    irqEnabled = (data & 0x02) != 0;
                    irqMode = (data & 0x04) != 0;

                    if (irqEnabled)
                    {
                        irqCounter = irqLatch;
                        irqPrescaler = 341;
                    }
                    irqActive = false;
                }
                else if (regOffset == 3)
                {
                    irqActive = false;
                    irqEnabled = irqEnableOnAck;
                }
                break;
            }
            return false;
        }

        return false;
    }

    bool Mapper023::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF)
        {
            uint8_t bankIdx = (addr >> 10) & 0x07;
            uint32_t bank = chrBank[bankIdx];

            uint32_t total1kBanks = nCHRBanks == 0 ? 8 : (nCHRBanks * 8);
            bank %= total1kBanks;

            mapped_addr = bank * 1024 + (addr & 0x03FF);
            return true;
        }
        return false;
    }

    bool Mapper023::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF)
        {
            if (nCHRBanks == 0)
            {
                uint8_t bankIdx = (addr >> 10) & 0x07;
                uint32_t bank = chrBank[bankIdx] % 8;
                mapped_addr = bank * 1024 + (addr & 0x03FF);
                return true;
            }
        }
        return false;
    }

    MirrorMode Mapper023::getMirrorMode()
    {
        return mirrorMode;
    }

    bool Mapper023::getIrqFlag() const
    {
        return irqActive;
    }

    void Mapper023::clearIrqFlag()
    {
        irqActive = false;
    }

    void Mapper023::tick()
    {
        if (irqEnabled)
        {
            if (irqMode)
            {
                // Modo ciclo CPU
                if (irqCounter == 0xFF)
                {
                    irqCounter = irqLatch;
                    irqActive = true;
                }
                else
                {
                    irqCounter++;
                }
            }
            else
            {
                // Modo prescaler (~114 ciclos de CPU = 341 PPU dots)
                irqPrescaler -= 3;
                if (irqPrescaler <= 0)
                {
                    irqPrescaler += 341;
                    if (irqCounter == 0xFF)
                    {
                        irqCounter = irqLatch;
                        irqActive = true;
                    }
                    else
                    {
                        irqCounter++;
                    }
                }
            }
        }
    }

    void Mapper023::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
        os.write(reinterpret_cast<const char *>(&ogMirrorMode), sizeof(ogMirrorMode));
        os.write(reinterpret_cast<const char *>(&prgBank0), sizeof(prgBank0));
        os.write(reinterpret_cast<const char *>(&prgBank1), sizeof(prgBank1));
        os.write(reinterpret_cast<const char *>(&prgMode), sizeof(prgMode));
        os.write(reinterpret_cast<const char *>(&prgRamEnable), sizeof(prgRamEnable));

        os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
        os.write(reinterpret_cast<const char *>(chrBank), sizeof(chrBank));
        os.write(reinterpret_cast<const char *>(chrBankRegLow), sizeof(chrBankRegLow));
        os.write(reinterpret_cast<const char *>(chrBankRegHigh), sizeof(chrBankRegHigh));

        os.write(reinterpret_cast<const char *>(&irqLatch), sizeof(irqLatch));
        os.write(reinterpret_cast<const char *>(&irqCounter), sizeof(irqCounter));
        os.write(reinterpret_cast<const char *>(&irqEnabled), sizeof(irqEnabled));
        os.write(reinterpret_cast<const char *>(&irqEnableOnAck), sizeof(irqEnableOnAck));
        os.write(reinterpret_cast<const char *>(&irqMode), sizeof(irqMode));
        os.write(reinterpret_cast<const char *>(&irqActive), sizeof(irqActive));
        os.write(reinterpret_cast<const char *>(&irqPrescaler), sizeof(irqPrescaler));
    }

    void Mapper023::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
        is.read(reinterpret_cast<char *>(&ogMirrorMode), sizeof(ogMirrorMode));
        is.read(reinterpret_cast<char *>(&prgBank0), sizeof(prgBank0));
        is.read(reinterpret_cast<char *>(&prgBank1), sizeof(prgBank1));
        is.read(reinterpret_cast<char *>(&prgMode), sizeof(prgMode));
        is.read(reinterpret_cast<char *>(&prgRamEnable), sizeof(prgRamEnable));

        is.read(reinterpret_cast<char *>(vPRGRAM), sizeof(vPRGRAM));
        is.read(reinterpret_cast<char *>(chrBank), sizeof(chrBank));
        is.read(reinterpret_cast<char *>(chrBankRegLow), sizeof(chrBankRegLow));
        is.read(reinterpret_cast<char *>(chrBankRegHigh), sizeof(chrBankRegHigh));

        is.read(reinterpret_cast<char *>(&irqLatch), sizeof(irqLatch));
        is.read(reinterpret_cast<char *>(&irqCounter), sizeof(irqCounter));
        is.read(reinterpret_cast<char *>(&irqEnabled), sizeof(irqEnabled));
        is.read(reinterpret_cast<char *>(&irqEnableOnAck), sizeof(irqEnableOnAck));
        is.read(reinterpret_cast<char *>(&irqMode), sizeof(irqMode));
        is.read(reinterpret_cast<char *>(&irqActive), sizeof(irqActive));
        is.read(reinterpret_cast<char *>(&irqPrescaler), sizeof(irqPrescaler));
    }
}