#pragma once
#include <cstdint>
#include "Common/Common.h" // Ou onde seu MirrorMode estiver definido
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class Mapper
    {
    public:
        Mapper(uint8_t prgBanks, uint8_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
        virtual ~Mapper() = default;

        // Transforma o endereço virtual da CPU em um endereço físico na PRG-ROM
        virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) = 0;
        virtual bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) = 0;

        // Transforma o endereço virtual da PPU em um endereço físico na CHR-ROM
        virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) = 0;
        virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) = 0;

        // Retorna se o Mapper está solicitando uma interrupção (IRQ)
        virtual bool getIrqFlag() const { return false; }

        // Limpa o flag de IRQ após ele ter sido servido (acknowledge)
        virtual void clearIrqFlag() {}

        // Realiza o tick
        virtual void tick() {}

        // Serialização para Save / Load states
        virtual void saveState(std::ostream &os) = 0;
        virtual void loadState(std::istream &is) = 0;

        // Define o estado inicial do mapper. Virtual puro não é necessário aqui,
        // permitindo que mappers simples não precisem implementá-lo.
        virtual void reset() {}

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