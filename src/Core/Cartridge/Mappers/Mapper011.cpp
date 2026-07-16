#include "Core/Cartridge/Mappers/Mapper011.h"

namespace R2NES::Core
{
    Mapper011::Mapper011(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) 
        : Mapper(prgBanks, chrBanks), mirrorMode(mirror) 
    {
        reset();
    }

    Mapper011::~Mapper011() {}

    bool Mapper011::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Mapper 11 maps 32KB PRG-ROM banks.
            // nPRGBanks is in 16KB units, so total PRG size is nPRGBanks * 16384.
            uint32_t offset = addr & 0x7FFF;
            uint32_t totalSize = static_cast<uint32_t>(nPRGBanks) * 16384;
            mapped_addr = (static_cast<uint32_t>(nPRGBankSelect) * 0x8000 + offset) % totalSize;
            return true;
        }
        return false;
    }

    bool Mapper011::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Data format: CCCC LLPP
            // Bits 0-1: 32KB PRG bank select
            // Bits 4-7: 8KB CHR bank select

            // nPRGBanks is in 16KB chunks. We have 32KB banks, so nPRGBanks/2 is the number of 32KB banks.
            uint8_t nPRG32kChunks = nPRGBanks / 2;
            if (nPRG32kChunks > 0)
                nPRGBankSelect = (data & 0x03) % nPRG32kChunks;
            else
                nPRGBankSelect = 0;

            if (nCHRBanks > 0)
                nCHRBankSelect = ((data >> 4) & 0x0F) % nCHRBanks;
            else
                nCHRBankSelect = 0;

            return true;
        }
        return false;
    }

    bool Mapper011::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            if (nCHRBanks == 0)
            {
                // Caso use CHR-RAM
                mapped_addr = addr;
                return true;
            }

            uint32_t offset = addr & 0x1FFF;
            uint32_t totalSize = static_cast<uint32_t>(nCHRBanks) * 8192;
            mapped_addr = (static_cast<uint32_t>(nCHRBankSelect) * 0x2000 + offset) % totalSize;
            return true;
        }
        return false;
    }

    bool Mapper011::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
        {
            // Permite escrita se estivermos usando CHR-RAM
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    void Mapper011::reset()
    {
        nPRGBankSelect = 0;
        nCHRBankSelect = 0;
    }

    MirrorMode Mapper011::getMirrorMode()
    {
        return mirrorMode;
    }

    void Mapper011::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
    }

    void Mapper011::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
    }
}