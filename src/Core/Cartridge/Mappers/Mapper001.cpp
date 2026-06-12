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
            if ((nPRGBankSelect & 0x10) == 0)
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
                uint8_t baseBank = (nPRGBankHigh << 4) | (nPRGBankSelect & 0x0E);
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (baseBank % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
                else
                    mapped_addr = ((baseBank | 0x01) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
            }
            else if (prgMode == 2)
            {
                // Modo 2: Fixa banco 0 em $8000-$BFFF, troca 16KB em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (nPRGBankHigh << 4) * 0x4000 + (addr & 0x3FFF); // Primeiro banco da região de 256KB
                else
                    mapped_addr = (((nPRGBankHigh << 4) | (nPRGBankSelect & 0x0F)) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
            }
            else // prgMode == 3
            {
                // Modo 3: Troca 16KB em $8000-$BFFF, fixa último banco em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (((nPRGBankHigh << 4) | (nPRGBankSelect & 0x0F)) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
                else
                {
                    // Banco fixo: último banco da região de 256KB (SUROM) ou último banco absoluto
                    uint32_t lastBank = (nPRGBankHigh << 4) | 0x0F;
                    if (lastBank >= nPRGBanks)
                        lastBank = nPRGBanks - 1;
                    mapped_addr = (lastBank * 0x4000) + (addr & 0x3FFF);
                }
            }
            return true;
        }
        return false;
    }

    bool Mapper001::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            // PRG RAM (8KB) - verificar se está habilitada (bit 4 do Control Register)
            if ((nPRGBankSelect & 0x10) == 0)
            {
                nPRGStaticRAM[addr & 0x1FFF] = data;
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            if (systemClockCounter - nLastWriteCycle <= 1)
            {
                nLastWriteCycle = systemClockCounter;
                return false;
            }
            nLastWriteCycle = systemClockCounter;

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
                    {
                        nCHRBankSelect0 = nShiftRegister & 0x1F;
                        // Em placas SUROM/SXROM, o bit 4 do banco CHR controla o PRG A18
                        if (nCHRBanks == 0)
                            nPRGBankHigh = (nShiftRegister & 0x10) >> 4;
                    }
                    else if (targetRegister == 2)
                    {
                        nCHRBankSelect1 = nShiftRegister & 0x1F;
                        if (nCHRBanks == 0)
                            nPRGBankHigh = (nShiftRegister & 0x10) >> 4;
                    }
                    else if (targetRegister == 3) // PRG Bank ($E000-$FFFF)
                        nPRGBankSelect = nShiftRegister & 0x1F;

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

    void Mapper001::reset()
    {
        nCHRBankSelect0 = 0x00;
        nCHRBankSelect1 = 0x00;
        nPRGBankSelect = 0x00;
        nControlRegister = 0x1C;
        nShiftRegister = 0x00;
        nShiftRegisterCount = 0x00;
        nLastWriteCycle = 0;
        nPRGBankHigh = 0;

        // Inicializa PRG RAM com zeros
        for (int i = 0; i < 0x2000; i++)
            nPRGStaticRAM[i] = 0x00;
    }

    void Mapper001::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nControlRegister), sizeof(nControlRegister));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect0), sizeof(nCHRBankSelect0));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect1), sizeof(nCHRBankSelect1));
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&nShiftRegister), sizeof(nShiftRegister));
        os.write(reinterpret_cast<const char *>(&nShiftRegisterCount), sizeof(nShiftRegisterCount));
        os.write(reinterpret_cast<const char *>(nPRGStaticRAM), sizeof(nPRGStaticRAM));
        os.write(reinterpret_cast<const char *>(&nLastWriteCycle), sizeof(nLastWriteCycle));
        os.write(reinterpret_cast<const char *>(&nPRGBankHigh), sizeof(nPRGBankHigh));
    }

    void Mapper001::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nControlRegister), sizeof(nControlRegister));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect0), sizeof(nCHRBankSelect0));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect1), sizeof(nCHRBankSelect1));
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&nShiftRegister), sizeof(nShiftRegister));
        is.read(reinterpret_cast<char *>(&nShiftRegisterCount), sizeof(nShiftRegisterCount));
        is.read(reinterpret_cast<char *>(nPRGStaticRAM), sizeof(nPRGStaticRAM));
        is.read(reinterpret_cast<char *>(&nLastWriteCycle), sizeof(nLastWriteCycle));
        is.read(reinterpret_cast<char *>(&nPRGBankHigh), sizeof(nPRGBankHigh));
    }
}