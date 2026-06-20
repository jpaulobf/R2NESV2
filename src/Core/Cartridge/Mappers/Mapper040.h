#pragma once
#include "Mapper.h"
#include <iostream>

namespace R2NES::Core
{
    class Mapper040 : public Mapper
    {
    public:
        Mapper040(uint8_t prgBanks, uint8_t chrBanks);
        ~Mapper040() override;

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;

        void saveState(std::ostream &os) override;
        void loadState(std::istream &is) override;

        // Retorna se o Mapper está solicitando uma interrupção (IRQ)
        bool getIrqFlag() const;

        // Limpa o flag de IRQ após ele ter sido servido (acknowledge)
        void clearIrqFlag();

        // Método que deve ser chamado a cada ciclo da CPU no seu Bus principal
        void tick();

    private:
        uint8_t mChrBanks;
        uint8_t prgBank_C000;
        bool bIRQEnabled = false;
        bool bIRQActive = false;
        uint32_t irqCounter;
    };
}