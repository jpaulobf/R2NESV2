#include "Core/Cartridge/Mappers/Mapper001.h"

namespace R2NES::Core
{
    Mapper001::Mapper001(uint8_t prgBanks, uint8_t chrBanks, uint8_t prgRamBanks)
        : Mapper(prgBanks, chrBanks), nPRGRAMBanks(prgRamBanks > 4 ? 4 : prgRamBanks)
    {
    }

    Mapper001::~Mapper001() {}

    bool Mapper001::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            if (nPRGRAMBanks != 0 && isPrgRamEnabled())
            {
                const uint32_t ramOffset = static_cast<uint32_t>(getPrgRamBank()) * 0x2000 + (addr & 0x1FFF);
                data = nPRGStaticRAM[ramOffset];
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            // Com a PRG RAM desabilitada, o barramento do cartucho fica aberto.
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            if (nPRGBanks == 0)
                return false;

            uint8_t prgMode = (nControlRegister >> 2) & 0x03;

            if (prgMode <= 1)
            {
                // Modo 0 ou 1: Switch 32KB (2 bancos de 16KB, bit 0 de nPRGBankSelect é ignorado)
                uint8_t baseBank = ((nPRGBankHigh & 0x01) << 4) | (nPRGBankSelect & 0x0E);
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (baseBank % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
                else
                    mapped_addr = ((baseBank | 0x01) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
            }
            else if (prgMode == 2)
            {
                // Modo 2: Fixa banco 0 em $8000-$BFFF, troca 16KB em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = (((nPRGBankHigh & 0x01) << 4) % nPRGBanks) * 0x4000 + (addr & 0x3FFF); // Primeiro banco da região de 256KB com módulo seguro
                else
                    mapped_addr = ((((nPRGBankHigh & 0x01) << 4) | (nPRGBankSelect & 0x0F)) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
            }
            else // prgMode == 3
            {
                // Modo 3: Troca 16KB em $8000-$BFFF, fixa último banco em $C000-$FFFF
                if (addr >= 0x8000 && addr <= 0xBFFF)
                    mapped_addr = ((((nPRGBankHigh & 0x01) << 4) | (nPRGBankSelect & 0x0F)) % nPRGBanks) * 0x4000 + (addr & 0x3FFF);
                else
                {
                    // Banco fixo: último banco da região de 256KB (SUROM) ou último banco absoluto
                    uint32_t lastBank = ((nPRGBankHigh & 0x01) << 4) | 0x0F;
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
            if (nPRGRAMBanks != 0 && isPrgRamEnabled())
            {
                const uint32_t ramOffset = static_cast<uint32_t>(getPrgRamBank()) * 0x2000 + (addr & 0x1FFF);
                nPRGStaticRAM[ramOffset] = data;
                mapped_addr = 0xFFFFFFFF;
                return true;
            }
            return false;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            const bool isShiftReset = (data & 0x80) != 0;
            const bool isConsecutive = (systemClockCounter - nLastWriteCycle) <= 1;

            // O MMC1 sempre aceita o reset pelo bit 7. Escritas consecutivas
            // sem reset não avançam o registrador serial.
            const bool acceptWrite = isShiftReset || !isConsecutive;
            nLastWriteCycle = systemClockCounter;
            if (!acceptWrite)
                return false;

            if (isShiftReset)
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
                    {
                        nControlRegister = nShiftRegister & 0x1F;
                        updateExtendedPrgBank();
                    }
                    else if (targetRegister == 1) // CHR Bank 0 ($A000-$BFFF)
                    {
                        nCHRBankSelect0 = nShiftRegister & 0x1F;
                        updateExtendedPrgBank();
                    }
                    else if (targetRegister == 2)
                    {
                        nCHRBankSelect1 = nShiftRegister & 0x1F;
                        updateExtendedPrgBank();
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

    bool Mapper001::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF)
        {
            uint32_t totalChrSize = nCHRBanks == 0 ? 8192 : nCHRBanks * 8192;
            uint8_t chrMode = (nControlRegister >> 4) & 0x01;

            if (chrMode == 0)
            {
                // Modo 0: 8KB switch (um banco de 8KB cobre $0000-$1FFF)
                // Bit 0 de nCHRBankSelect0 é ignorado neste modo
                mapped_addr = ((nCHRBankSelect0 & 0x1E) * 0x1000 + (addr & 0x1FFF)) % totalChrSize;
            }
            else
            {
                // Modo 1: 4KB + 4KB (dois bancos de 4KB: $0000-$0FFF e $1000-$1FFF)
                if (addr <= 0x0FFF)
                    mapped_addr = ((nCHRBankSelect0 & 0x1F) * 0x1000 + (addr & 0x0FFF)) % totalChrSize;
                else
                    mapped_addr = ((nCHRBankSelect1 & 0x1F) * 0x1000 + (addr & 0x0FFF)) % totalChrSize;
            }
            return true;
        }
        return false;
    }

    bool Mapper001::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF)
        {
            if (nCHRBanks == 0) // Permite escrita se for CHR RAM
            {
                uint32_t totalChrSize = 8192; // Base de 8KB padrão, adaptado se houver headers modernos
                uint8_t chrMode = (nControlRegister >> 4) & 0x01;

                if (chrMode == 0)
                    mapped_addr = ((nCHRBankSelect0 & 0x1E) * 0x1000 + (addr & 0x1FFF)) % totalChrSize;
                else
                {
                    if (addr <= 0x0FFF)
                        mapped_addr = ((nCHRBankSelect0 & 0x1F) * 0x1000 + (addr & 0x0FFF)) % totalChrSize;
                    else
                        mapped_addr = ((nCHRBankSelect1 & 0x1F) * 0x1000 + (addr & 0x0FFF)) % totalChrSize;
                }
                return true;
            }
        }
        return false;
    }

    void Mapper001::onPpuAddress(uint16_t addr, uint32_t)
    {
        // Palette RAM is internal to the PPU. Pattern and nametable accesses
        // carry the A12 value that selects the active MMC1 CHR register.
        if (addr >= 0x3F00)
            return;

        const bool ppuA12 = (addr & 0x1000) != 0;
        if (ppuA12)
            nPRGBankHigh |= 0x80;
        else
            nPRGBankHigh &= 0x7F;

        updateExtendedPrgBank();
    }

    uint8_t Mapper001::getActiveChrBankRegister() const
    {
        if ((nControlRegister & 0x10) != 0 && (nPRGBankHigh & 0x80) != 0)
            return nCHRBankSelect1;
        return nCHRBankSelect0;
    }

    void Mapper001::updateExtendedPrgBank()
    {
        nPRGBankHigh &= 0x80;
        if (nCHRBanks == 0 && nPRGBanks > 16 && (getActiveChrBankRegister() & 0x10))
            nPRGBankHigh |= 0x01;
    }

    bool Mapper001::isPrgRamEnabled() const
    {
        // MMC1B bit 4 desabilita a PRG RAM. Em SNROM, CHR.4 também pode
        // desabilitá-la; em SUROM/SXROM, esse pino seleciona PRG A18.
        const bool disabledByPrgRegister = (nPRGBankSelect & 0x10) != 0;
        const bool disabledBySnrom = nPRGRAMBanks == 1 && nCHRBanks == 0 && nPRGBanks <= 16 &&
                                     (getActiveChrBankRegister() & 0x10) != 0;
        return !disabledByPrgRegister && !disabledBySnrom;
    }

    uint8_t Mapper001::getPrgRamBank() const
    {
        if (nPRGRAMBanks <= 1)
            return 0;

        const uint8_t chrBank = getActiveChrBankRegister();

        // SZROM reutiliza CHR0.4 para selecionar a segunda página de PRG RAM.
        if (nCHRBanks != 0)
            return nPRGRAMBanks == 2 ? (chrBank >> 4) & 0x01 : 0;

        if (nPRGRAMBanks == 2)
            return (chrBank >> 3) & 0x01;

        // SXROM usa CHR0.2 como A13 e CHR0.3 como A14 da PRG RAM.
        const uint8_t bank = ((chrBank >> 2) & 0x01) |
                             (((chrBank >> 3) & 0x01) << 1);
        return bank % nPRGRAMBanks;
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
        nControlRegister = 0x1E;
        nShiftRegister = 0x00;
        nShiftRegisterCount = 0x00;
        nLastWriteCycle = 0;
        nPRGBankHigh = 0;

        // PRG RAM pode conter dados de bateria; reset do console não a apaga.
    }

    void Mapper001::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nControlRegister), sizeof(nControlRegister));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect0), sizeof(nCHRBankSelect0));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect1), sizeof(nCHRBankSelect1));
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&nShiftRegister), sizeof(nShiftRegister));
        os.write(reinterpret_cast<const char *>(&nShiftRegisterCount), sizeof(nShiftRegisterCount));
        os.write(reinterpret_cast<const char *>(nPRGStaticRAM),
                 static_cast<std::streamsize>(nPRGRAMBanks) * 0x2000);
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
        is.read(reinterpret_cast<char *>(nPRGStaticRAM),
                static_cast<std::streamsize>(nPRGRAMBanks) * 0x2000);
        is.read(reinterpret_cast<char *>(&nLastWriteCycle), sizeof(nLastWriteCycle));
        is.read(reinterpret_cast<char *>(&nPRGBankHigh), sizeof(nPRGBankHigh));
    }

}