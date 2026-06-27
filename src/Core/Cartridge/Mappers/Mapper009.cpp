#include "Core/Cartridge/Mappers/Mapper009.h"

namespace R2NES::Core
{
    Mapper009::Mapper009(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror)
        : Mapper(prgBanks, chrBanks)
    {
        mirrorMode = mirror;
        ogMirrorMode = mirror;
        reset();
    }

    Mapper009::~Mapper009() {}

    void Mapper009::reset()
    {
        nPRGBankSelect = 0;
        nCHRBankSelect_0_FD = 0;
        nCHRBankSelect_0_FE = 0;
        nCHRBankSelect_1_FD = 0;
        nCHRBankSelect_1_FE = 0;

        latch0 = LatchState::FE;
        latch1 = LatchState::FE;

        mirrorMode = ogMirrorMode;

        for (int i = 0; i < 8192; ++i)
        {
            vPRGRAM[i] = 0x00;
        }
    }

    bool Mapper009::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            data = vPRGRAM[addr & 0x1FFF];
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint32_t num_8k_banks = nPRGBanks * 2;
            if (num_8k_banks == 0)
                num_8k_banks = 4; // Fallback de segurança

            if (addr >= 0x8000 && addr <= 0x9FFF)
            {
                mapped_addr = (nPRGBankSelect % num_8k_banks) * 8192 + (addr & 0x1FFF);
            }
            else if (addr >= 0xA000 && addr <= 0xBFFF)
            {
                mapped_addr = ((num_8k_banks - 3) % num_8k_banks) * 8192 + (addr & 0x1FFF);
            }
            else if (addr >= 0xC000 && addr <= 0xDFFF)
            {
                mapped_addr = ((num_8k_banks - 2) % num_8k_banks) * 8192 + (addr & 0x1FFF);
            }
            else // addr >= 0xE000 && addr <= 0xFFFF
            {
                mapped_addr = ((num_8k_banks - 1) % num_8k_banks) * 8192 + (addr & 0x1FFF);
            }
            return true;
        }

        return false;
    }

    bool Mapper009::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            vPRGRAM[addr & 0x1FFF] = data;
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            if (addr >= 0xA000 && addr <= 0xAFFF)
            {
                nPRGBankSelect = data & 0x0F;
            }
            else if (addr >= 0xB000 && addr <= 0xBFFF)
            {
                nCHRBankSelect_0_FD = data & 0x1F;
            }
            else if (addr >= 0xC000 && addr <= 0xCFFF)
            {
                nCHRBankSelect_0_FE = data & 0x1F;
            }
            else if (addr >= 0xD000 && addr <= 0xDFFF)
            {
                nCHRBankSelect_1_FD = data & 0x1F;
            }
            else if (addr >= 0xE000 && addr <= 0xEFFF)
            {
                nCHRBankSelect_1_FE = data & 0x1F;
            }
            else if (addr >= 0xF000 && addr <= 0xFFFF)
            {
                mirrorMode = (data & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
            }
        }

        return false;
    }

    bool Mapper009::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            uint32_t num_4k_banks = nCHRBanks * 2;
            if (num_4k_banks == 0)
                num_4k_banks = 2; // Fallback de segurança se CHR for 8KB ou RAM

            uint8_t selected_bank = 0;

            if (addr >= 0x0000 && addr <= 0x0FFF)
            {
                selected_bank = (latch0 == LatchState::FD) ? nCHRBankSelect_0_FD : nCHRBankSelect_0_FE;
                mapped_addr = (selected_bank % num_4k_banks) * 4096 + (addr & 0x0FFF);

                // Latch atualiza APÓS o ciclo de leitura (para a próxima leitura)
                if (addr == 0x0FD8)
                {
                    latch0 = LatchState::FD;
                }
                else if (addr == 0x0FE8)
                {
                    latch0 = LatchState::FE;
                }
            }
            else // addr >= 0x1000 && addr <= 0x1FFF
            {
                selected_bank = (latch1 == LatchState::FD) ? nCHRBankSelect_1_FD : nCHRBankSelect_1_FE;
                mapped_addr = (selected_bank % num_4k_banks) * 4096 + (addr & 0x0FFF);

                // Latch atualiza APÓS o ciclo de leitura
                if (addr >= 0x1FD8 && addr <= 0x1FDF)
                {
                    latch1 = LatchState::FD;
                }
                else if (addr >= 0x1FE8 && addr <= 0x1FEF)
                {
                    latch1 = LatchState::FE;
                }
            }
            return true;
        }

        return false;
    }

    bool Mapper009::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            if (nCHRBanks == 0) // Se for CHR RAM (por segurança e completude)
            {
                uint8_t selected_bank = 0;
                if (addr >= 0x0000 && addr <= 0x0FFF)
                {
                    selected_bank = (latch0 == LatchState::FD) ? nCHRBankSelect_0_FD : nCHRBankSelect_0_FE;
                    mapped_addr = selected_bank * 4096 + (addr & 0x0FFF);
                }
                else
                {
                    selected_bank = (latch1 == LatchState::FD) ? nCHRBankSelect_1_FD : nCHRBankSelect_1_FE;
                    mapped_addr = selected_bank * 4096 + (addr & 0x0FFF);
                }
                return true;
            }
        }
        return false;
    }

    MirrorMode Mapper009::getMirrorMode()
    {
        return mirrorMode;
    }

    void Mapper009::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect_0_FD), sizeof(nCHRBankSelect_0_FD));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect_0_FE), sizeof(nCHRBankSelect_0_FE));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect_1_FD), sizeof(nCHRBankSelect_1_FD));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect_1_FE), sizeof(nCHRBankSelect_1_FE));

        uint8_t l0 = static_cast<uint8_t>(latch0);
        uint8_t l1 = static_cast<uint8_t>(latch1);
        os.write(reinterpret_cast<const char *>(&l0), sizeof(l0));
        os.write(reinterpret_cast<const char *>(&l1), sizeof(l1));

        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
        os.write(reinterpret_cast<const char *>(&ogMirrorMode), sizeof(ogMirrorMode));

        os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
    }

    void Mapper009::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect_0_FD), sizeof(nCHRBankSelect_0_FD));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect_0_FE), sizeof(nCHRBankSelect_0_FE));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect_1_FD), sizeof(nCHRBankSelect_1_FD));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect_1_FE), sizeof(nCHRBankSelect_1_FE));

        uint8_t l0 = 0, l1 = 0;
        is.read(reinterpret_cast<char *>(&l0), sizeof(l0));
        is.read(reinterpret_cast<char *>(&l1), sizeof(l1));
        latch0 = static_cast<LatchState>(l0);
        latch1 = static_cast<LatchState>(l1);

        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
        is.read(reinterpret_cast<char *>(&ogMirrorMode), sizeof(ogMirrorMode));

        is.read(reinterpret_cast<char *>(vPRGRAM), sizeof(vPRGRAM));
    }
}