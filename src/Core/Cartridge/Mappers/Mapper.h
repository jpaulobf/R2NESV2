#pragma once
#include <cstdint>

namespace R2NES::Core
{
    class Mapper
    {
    public:
        Mapper(uint8_t prgBanks, uint8_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
        virtual ~Mapper() = default;

        // Transforma o endereço virtual da CPU em um endereço físico na PRG-ROM
        virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr) = 0;
        virtual bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr) = 0;

        // Transforma o endereço virtual da PPU em um endereço físico na CHR-ROM
        virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr) = 0;
        virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr) = 0;

    protected:
        uint8_t nPRGBanks = 0;
        uint8_t nCHRBanks = 0;
    };
}