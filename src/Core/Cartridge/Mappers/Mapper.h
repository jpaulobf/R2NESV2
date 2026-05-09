#pragma once
#include <cstdint>
#include "Common/Common.h" // Ou onde seu MirrorMode estiver definido

namespace R2NES::Core
{
    class Mapper
    {
    public:
        Mapper(uint8_t prgBanks, uint8_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
        virtual ~Mapper() = default;

        // Transforma o endereço virtual da CPU em um endereço físico na PRG-ROM
        virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) = 0;
        virtual bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) = 0;

        // Transforma o endereço virtual da PPU em um endereço físico na CHR-ROM
        virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) = 0;
        virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) = 0;

        // Por padrão, mappers retornam HARDWARE (indicando o que está no iNES)
        virtual MirrorMode getMirrorMode() 
        { 
            return MirrorMode::HORIZONTAL; // Fallback padrão
        }

    protected:
        uint8_t nPRGBanks = 0;
        uint8_t nCHRBanks = 0;
    };
}