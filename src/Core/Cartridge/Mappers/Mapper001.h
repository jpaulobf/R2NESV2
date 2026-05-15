#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    /*
     * MMC1 (Nintendo SxROM) - Um dos mappers mais populares do NES
     * 
     * Registros de escrita ($8000-$FFFF):
     * - Bit 7: Reset (quando 1, reseta o shift register)
     * - Bits 0-4: Dados para o shift register (carregados em série)
     * - Bits 13-14 do endereço: Seleciona qual registrador recebe os dados
     * 
     * Registrador de Controle ($8000-$9FFF):
     * - Bits 0-1: Modo de espelhamento (0-3)
     * - Bits 2-3: Modo de seleção PRG (0-3)
     * - Bit 4: Habilita PRG RAM (0=habilitado, 1=desabilitado)
     * - Bit 4: Modo CHR (quando estiver em bits 4)
     * 
     * Modos PRG:
     * - 0-1: Switch 32KB
     * - 2: Fixa banco 0 em $8000-$BFFF, troca 16KB em $C000-$FFFF
     * - 3: Troca 16KB em $8000-$BFFF, fixa último banco em $C000-$FFFF
     * 
     * Modos CHR:
     * - 0: Switch 8KB
     * - 1: Switch 4KB + 4KB (separados)
     */
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
        // Seletores de banco
        uint8_t nCHRBankSelect0 = 0x00;  // Banco CHR para $0000-$0FFF (modo 0) ou $0000-$0FFF (modo 1)
        uint8_t nCHRBankSelect1 = 0x00;  // Banco CHR para $1000-$1FFF (modo 1 apenas)
        uint8_t nPRGBankSelect = 0x00;   // Seletor de banco PRG (seletivo dependendo do modo)

        uint8_t nControlRegister = 0x1C;  // Valor inicial padrão (PRG Mode 3, Vertical Mirroring)
        
        // Shift register para serial writing dos registros
        uint8_t nShiftRegister = 0x00;
        uint8_t nShiftRegisterCount = 0x00;

        uint8_t nPRGStaticRAM[32768];  // PRG RAM (8KB) - alguns cartuchos (como Metroid) usam isso
    };
}