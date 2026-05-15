#include "Core/Cartridge/Mappers/Mapper001.h"

namespace R2NES::Core
{
    Mapper001::Mapper001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks)
    {
        // Inicializa a PRG RAM se necessário
        for (int i = 0; i < 32768; i++)
            nPRGStaticRAM[i] = 0x00;
    }

    Mapper001::~Mapper001() {}

    bool Mapper001::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            // PRG RAM (8KB) - verificar se está habilitada (bit 4 do Control Register)
            if ((nControlRegister & 0x10) == 0) // RAM habilitada quando bit 4 é 0
            {
                data = nPRGStaticRAM[addr & 0x1FFF];
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            // Se desabilitada, cartuccho deixa barramento aberto (retorna valor anterior)
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint8_t prgMode = (nControlRegister >> 2) & 0x03;

            if (prgMode <= 1)
            {
                // Modo 0 ou 1: Switch 32KB (2 bancos de 16KB, bit 0 de nPRGBankSelect é ignorado)
                uint8_t baseBank = nPRGBankSelect & 0x0E;
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = baseBank * 0x4000 + (addr & 0x3FFF);
                else
                    mapped_addr = (baseBank | 0x01) * 0x4000 + (addr & 0x3FFF);
            }
            else if (prgMode == 2)
            {
                // Modo 2: Fixa banco 0 em $8000-$BFFF, troca 16KB em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = 0x0000 + (addr & 0x3FFF); // Sempre banco 0
                else
                    mapped_addr = (nPRGBankSelect & 0x0F) * 0x4000 + (addr & 0x3FFF);
            }
            else // prgMode == 3
            {
                // Modo 3: Troca 16KB em $8000-$BFFF, fixa último banco em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (nPRGBankSelect & 0x0F) * 0x4000 + (addr & 0x3FFF);
                else
                    mapped_addr = ((nPRGBanks - 1) & 0x0F) * 0x4000 + (addr & 0x3FFF);
            }
            return true;
        }
        return false;
    }

    bool Mapper001::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            // PRG RAM (8KB) - verificar se está habilitada (bit 4 do Control Register)
            if ((nControlRegister & 0x10) == 0) // RAM habilitada quando bit 4 é 0
            {
                nPRGStaticRAM[addr & 0x1FFF] = data;
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            if (data & 0x80) // Bit 7 = Reset do Shift Register
            {
                nShiftRegister = 0x00;
                nShiftRegisterCount = 0;
                // Quando reseta, Control Register recebe modo padrão: PRG Mode 3, Vertical Mirroring
                nControlRegister |= 0x0C; // Bits 2-3 para modo 3
            }
            else
            {
                // Shiftar bit 0 do dado para o shift register (LSB first)
                nShiftRegister >>= 1;
                nShiftRegister |= (data & 0x01) << 4;
                nShiftRegisterCount++;

                // Quando 5 bits foram carregados, escrever no registrador apropriado
                if (nShiftRegisterCount == 5)
                {
                    // Bits 13-14 do endereço determinam qual registrador é escrito
                    uint8_t targetRegister = (addr >> 13) & 0x03;

                    if (targetRegister == 0) // Control Register ($8000-$9FFF)
                        nControlRegister = nShiftRegister & 0x1F;
                    else if (targetRegister == 1) // CHR Bank 0 ($A000-$BFFF)
                        nCHRBankSelect0 = nShiftRegister & 0x1F;
                    else if (targetRegister == 2) // CHR Bank 1 ($C000-$DFFF)
                        nCHRBankSelect1 = nShiftRegister & 0x1F;
                    else if (targetRegister == 3) // PRG Bank ($E000-$FFFF)
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
            if (nCHRBanks == 0) // CHR RAM (cartuchos com CHR RAM em vez de CHR ROM)
            {
                mapped_addr = addr;
                return true;
            }

            uint8_t chrMode = (nControlRegister >> 4) & 0x01;
            if (chrMode == 0)
            {
                // Modo 0: 8KB switch (um banco de 8KB cobre $0000-$1FFF)
                // Bit 0 de nCHRBankSelect0 é ignorado neste modo
                mapped_addr = (nCHRBankSelect0 & 0x1E) * 0x1000 + (addr & 0x1FFF);
            }
            else
            {
                // Modo 1: 4KB + 4KB (dois bancos de 4KB: $0000-$0FFF e $1000-$1FFF)
                if (addr <= 0x0FFF)
                    mapped_addr = (nCHRBankSelect0 & 0x1F) * 0x1000 + (addr & 0x0FFF);
                else
                    mapped_addr = (nCHRBankSelect1 & 0x1F) * 0x1000 + (addr & 0x0FFF);
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
        // Bits 0-1 do Control Register determinam o modo de espelhamento de nametable:
        // 0: One-screen (Low: $2000)
        // 1: One-screen (High: $2400)
        // 2: Vertical mirroring (VRAM A10 = PPU A10)
        // 3: Horizontal mirroring (VRAM A11 = PPU A10)
        switch (nControlRegister & 0x03)
        {
        case 0:
            return MirrorMode::ONESCREEN_LO;
        case 1:
            return MirrorMode::ONESCREEN_HI;
        case 2:
            return MirrorMode::VERTICAL;
        case 3:
            return MirrorMode::HORIZONTAL;
        }
        return MirrorMode::HORIZONTAL;
    }
}