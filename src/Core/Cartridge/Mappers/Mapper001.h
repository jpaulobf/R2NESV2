#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    class Mapper001 : public Mapper
    {
    public:
        Mapper001(uint8_t prgBanks, uint8_t chrBanks);
        ~Mapper001();

        bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;
        bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
        bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data) override;

        MirrorMode getMirrorMode() override;

    private:
        // Variáveis de estado do MMC1
        uint8_t nCHRBankSelect0 = 0x00;
        uint8_t nCHRBankSelect1 = 0x00;
        uint8_t nPRGBankSelect = 0x00;

        uint8_t nControlRegister = 0x1C; // Valor inicial padrão (PRG Mode 3)
        
        uint8_t nShiftRegister = 0x00;
        uint8_t nShiftRegisterCount = 0x00;

        uint8_t nPRGStaticRAM[32768]; // Algumas variantes usam PRG RAM
    };
}