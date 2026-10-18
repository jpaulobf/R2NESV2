#include "Core/Cartridge/Mappers/Mapper001.h"
#include <stdexcept>
#include <algorithm>

namespace R2NES::Core
{
    Mapper001::Mapper001(uint8_t prgBanks, uint8_t chrBanks, uint8_t prgRamBanks)
        : Mapper001(prgBanks, chrBanks, Configuration{uint32_t(prgRamBanks) * 8192, 0, 0, MirrorMode::HORIZONTAL})
    {
    }

    bool Mapper001::supports(uint16_t prgBanks, uint16_t chrBanks, const Configuration &config)
    {
        const uint64_t ram = uint64_t(config.prgRamSize) + config.prgNvRamSize;
        if (prgBanks == 0 || prgBanks > 32 || (prgBanks & (prgBanks - 1)) ||
            chrBanks > 16 || (chrBanks && (chrBanks & (chrBanks - 1))) ||
            config.prgRamSize % 8192 || config.prgNvRamSize % 8192 ||
            (ram != 0 && ram != 8192 && ram != 16384 && ram != 32768) ||
            (config.hardwareMirror != MirrorMode::HORIZONTAL && config.hardwareMirror != MirrorMode::VERTICAL))
            return false;
        // CHR A16 só está disponível para PRG externa quando CHR tem até 8 KiB.
        if ((prgBanks > 16 && chrBanks > 1) || (ram == 32768 && chrBanks > 1) ||
            (ram == 16384 && chrBanks > 8))
            return false;
        switch (config.submapper)
        {
        case 0:
            return true;
        case 1:
            return prgBanks == 32 && chrBanks <= 1 && ram == 8192; // SUROM depreciado.
        case 2:
            return prgBanks <= 16 && chrBanks <= 1 && ram == 16384; // SOROM depreciado.
        case 4:
            return chrBanks <= 1 && ram == 32768; // SXROM depreciado.
        case 5:
            return prgBanks == 2 && ram == 0;
        case 7:
            return prgBanks <= 16;
        default:
            return false; // MMC1A (3/mapper 155), EEPROM 2ME (6) e desconhecidos.
        }
    }

    Mapper001::Mapper001(uint16_t prgBanks, uint16_t chrBanks, Configuration config)
        : Mapper(prgBanks, chrBanks), configuration(config)
    {
        if (!supports(prgBanks, chrBanks, config))
            throw std::invalid_argument("Unsupported MMC1 memory/submapper configuration");
        nPRGRAMBanks = (config.prgRamSize + config.prgNvRamSize) / 8192;
        reset();
    }

    Mapper001::~Mapper001() {}

    std::vector<uint8_t> Mapper001::getBatteryData() const
    {
        // SOROM/SZROM: RAM volátil primeiro, segundo chip ligado à bateria.
        const auto start = nPRGStaticRAM + configuration.prgRamSize;
        return {start, start + configuration.prgNvRamSize};
    }

    bool Mapper001::setBatteryData(const std::vector<uint8_t> &data)
    {
        if (data.size() != configuration.prgNvRamSize)
            return false;
        std::copy(data.begin(), data.end(), nPRGStaticRAM + configuration.prgRamSize);
        return true;
    }

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

            if (configuration.submapper == 5)
            {
                mapped_addr = addr & 0x7FFF;
                return true;
            }

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
        const bool isConsecutive = hasLastWrite && (systemClockCounter - nLastWriteCycle) <= 1;
        // O cartucho observa R/W também fora do porto serial.
        nLastWriteCycle = systemClockCounter;
        hasLastWrite = true;
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

            // O MMC1 sempre aceita o reset pelo bit 7. Escritas consecutivas
            // sem reset não avançam o registrador serial.
            const bool acceptWrite = isShiftReset || !isConsecutive;
            mapped_addr = 0xFFFFFFFF;
            if (!acceptWrite)
                return true;

            if (isShiftReset)
            {
                nShiftRegister = 0x00;
                nShiftRegisterCount = 0;
                // Reset serial força PRG Mode 3, preservando espelhamento e modo CHR.
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
            return true;
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
                uint32_t totalChrSize = 8192; // Capacidades diferentes são rejeitadas pelo carregador.
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
        if (nCHRBanks <= 1 && nPRGBanks > 16 && (getActiveChrBankRegister() & 0x10))
            nPRGBankHigh |= 0x01;
    }

    bool Mapper001::isPrgRamEnabled() const
    {
        // MMC1B bit 4 desabilita a PRG RAM. Em SNROM, CHR.4 também pode
        // desabilitá-la; em SUROM/SXROM, esse pino seleciona PRG A18.
        const bool disabledByPrgRegister = (nPRGBankSelect & 0x10) != 0;
        const bool disabledBySnrom = nPRGRAMBanks == 1 && nCHRBanks <= 1 && nPRGBanks <= 16 &&
                                     (getActiveChrBankRegister() & 0x10) != 0;
        return !disabledByPrgRegister && !disabledBySnrom;
    }

    uint8_t Mapper001::getPrgRamBank() const
    {
        if (nPRGRAMBanks <= 1)
            return 0;

        const uint8_t chrBank = getActiveChrBankRegister();

        // SZROM reutiliza CHR0.4 para selecionar a segunda página de PRG RAM.
        if (nCHRBanks > 1)
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
        if (configuration.submapper == 7)
            return configuration.hardwareMirror;
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
        hasLastWrite = false;
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
        // Bit 6 antes reservado: conserva o tamanho dos estados antigos e aceita ciclo zero.
        const uint8_t packedHigh = nPRGBankHigh | (hasLastWrite ? 0x40 : 0);
        os.write(reinterpret_cast<const char *>(&packedHigh), sizeof(packedHigh));
    }

    void Mapper001::loadState(std::istream &is)
    {
        Mapper001 candidate(*this);
        is.read(reinterpret_cast<char *>(&candidate.nControlRegister), sizeof(nControlRegister));
        is.read(reinterpret_cast<char *>(&candidate.nCHRBankSelect0), sizeof(nCHRBankSelect0));
        is.read(reinterpret_cast<char *>(&candidate.nCHRBankSelect1), sizeof(nCHRBankSelect1));
        is.read(reinterpret_cast<char *>(&candidate.nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&candidate.nShiftRegister), sizeof(nShiftRegister));
        is.read(reinterpret_cast<char *>(&candidate.nShiftRegisterCount), sizeof(nShiftRegisterCount));
        is.read(reinterpret_cast<char *>(candidate.nPRGStaticRAM),
                static_cast<std::streamsize>(nPRGRAMBanks) * 0x2000);
        is.read(reinterpret_cast<char *>(&candidate.nLastWriteCycle), sizeof(nLastWriteCycle));
        is.read(reinterpret_cast<char *>(&candidate.nPRGBankHigh), sizeof(nPRGBankHigh));
        if (!is)
            return;
        const uint8_t count = candidate.nShiftRegisterCount;
        if (candidate.nControlRegister > 31 || candidate.nCHRBankSelect0 > 31 ||
            candidate.nCHRBankSelect1 > 31 || candidate.nPRGBankSelect > 31 || count > 4 ||
            candidate.nShiftRegister > 31 || (candidate.nPRGBankHigh & ~0xC1) ||
            (candidate.nShiftRegister & ((1u << (5 - count)) - 1)))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        candidate.hasLastWrite = (candidate.nPRGBankHigh & 0x40) || candidate.nLastWriteCycle != 0;
        candidate.nPRGBankHigh &= 0x80;
        candidate.updateExtendedPrgBank();
        *this = candidate;
    }
}
