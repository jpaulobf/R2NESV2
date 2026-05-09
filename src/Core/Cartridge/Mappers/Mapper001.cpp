#include "Core/Cartridge/Mappers/Mapper001.h"

namespace R2NES::Core
{
    Mapper001::Mapper001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) 
    {
        // Inicializa a PRG RAM se necessário
        for (int i = 0; i < 32768; i++) nPRGStaticRAM[i] = 0x00;
    }

    Mapper001::~Mapper001() {}

    bool Mapper001::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            // PRG RAM
            data = nPRGStaticRAM[addr & 0x1FFF];
            mapped_addr = 0xFFFFFFFF; 
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint8_t prgMode = (nControlRegister >> 2) & 0x03;

            if (prgMode <= 1)
            {
                // Modo 0 ou 1: Switch 32KB em $8000
                mapped_addr = (nPRGBankSelect & 0x0E) * 0x4000 + (addr & 0x7FFF);
            }
            else if (prgMode == 2)
            {
                // Modo 2: Fixa primeiro banco em $8000, troca 16KB em $C000
                if (addr >= 0x8000 && addr <= 0xBFFF) mapped_addr = addr & 0x3FFF;
                else mapped_addr = nPRGBankSelect * 0x4000 + (addr & 0x3FFF);
            }
            else
            {
                // Modo 3: Troca 16KB em $8000, fixa último banco em $C000
                if (addr >= 0x8000 && addr <= 0xBFFF) mapped_addr = nPRGBankSelect * 0x4000 + (addr & 0x3FFF);
                else mapped_addr = (nPRGBanks - 1) * 0x4000 + (addr & 0x3FFF);
            }
            return true;
        }
        return false;
    }

    bool Mapper001::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            nPRGStaticRAM[addr & 0x1FFF] = data;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            if (data & 0x80) // Reset do Shift Register
            {
                nShiftRegister = 0x00;
                nShiftRegisterCount = 0;
                nControlRegister |= 0x0C;
            }
            else
            {
                // Shiftar o bit 0 do dado para o registrador
                nShiftRegister >>= 1;
                nShiftRegister |= (data & 0x01) << 4;
                nShiftRegisterCount++;

                if (nShiftRegisterCount == 5)
                {
                    uint8_t targetRegister = (addr >> 13) & 0x03;

                    if (targetRegister == 0) // Control
                        nControlRegister = nShiftRegister & 0x1F;
                    else if (targetRegister == 1) // CHR Bank 0
                        nCHRBankSelect0 = nShiftRegister & 0x1F;
                    else if (targetRegister == 2) // CHR Bank 1
                        nCHRBankSelect1 = nShiftRegister & 0x1F;
                    else if (targetRegister == 3) // PRG Bank
                        nPRGBankSelect = nShiftRegister & 0x0F;

                    nShiftRegister = 0x00;
                    nShiftRegisterCount = 0;
                }
            }
        }
        return false;
    }

    bool Mapper001::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr <= 0x1FFF)
        {
            if (nCHRBanks == 0) // CHR RAM
            {
                mapped_addr = addr;
                return true;
            }

            uint8_t chrMode = (nControlRegister >> 4) & 0x01;
            if (chrMode == 0)
            {
                // Modo 8KB
                mapped_addr = (nCHRBankSelect0 & 0x1E) * 0x1000 + (addr & 0x1FFF);
            }
            else
            {
                // Modo 4KB
                if (addr <= 0x0FFF) mapped_addr = nCHRBankSelect0 * 0x1000 + (addr & 0x0FFF);
                else mapped_addr = nCHRBankSelect1 * 0x1000 + (addr & 0x0FFF);
            }
            return true;
        }
        return false;
    }

    bool Mapper001::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr <= 0x1FFF)
        {
            if (nCHRBanks == 0) // Permite escrita se for CHR RAM
            {
                mapped_addr = addr;
                return true;
            }
        }
        return false;
    }

    MirrorMode Mapper001::getMirrorMode()
    {
        // Bits 0 e 1 do Control Register determinam o mirroring
        switch (nControlRegister & 0x03)
        {
            case 0: return MirrorMode::ONESCREEN_LO;
            case 1: return MirrorMode::ONESCREEN_HI;
            case 2: return MirrorMode::VERTICAL;
            case 3: return MirrorMode::HORIZONTAL;
        }
        return MirrorMode::HORIZONTAL;
    }
}