#include "Core/Cartridge/Mappers/Mapper040.h"

namespace R2NES::Core
{
    Mapper040::Mapper040(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks), mChrBanks(chrBanks)
    {
        // Estado inicial com zero
        prgBank_C000 = 0;
        bIRQEnabled = false;
        bIRQActive = false;
        irqCounter = 0;
    }

    Mapper040::~Mapper040() {}

    bool Mapper040::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        // PRG Layout (Bancos de 8KB):
        // $6000-$7FFF: Banco Fixo 6
        // $8000-$9FFF: Banco Fixo 4
        // $A000-$BFFF: Banco Fixo 5
        // $C000-$DFFF: Banco Chaveável via reg $E000
        // $E000-$FFFF: Banco Fixo 7

        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = (6 * 0x2000) + (addr & 0x1FFF);
            return true;
        }
        if (addr >= 0x8000 && addr <= 0x9FFF)
        {
            mapped_addr = (4 * 0x2000) + (addr & 0x1FFF);
            return true;
        }
        if (addr >= 0xA000 && addr <= 0xBFFF)
        {
            mapped_addr = (5 * 0x2000) + (addr & 0x1FFF);
            return true;
        }
        if (addr >= 0xC000 && addr <= 0xDFFF)
        {
            mapped_addr = (prgBank_C000 * 0x2000) + (addr & 0x1FFF);
            return true;
        }
        if (addr >= 0xE000 && addr <= 0xFFFF)
        {
            mapped_addr = (7 * 0x2000) + (addr & 0x1FFF);
            return true;
        }

        return false;
    }

    bool Mapper040::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x8000 && addr <= 0x9FFF)
        {
            // $8000: Desabilita IRQ, zera e reconhece interrupção
            bIRQEnabled = false;
            bIRQActive = false;
            irqCounter = 0;
            return true;
        }
        if (addr >= 0xA000 && addr <= 0xBFFF)
        {
            // $A000: Habilita o contador de IRQ
            bIRQEnabled = true;
            return true;
        }
        if (addr >= 0xE000 && addr <= 0xFFFF)
        {
            // $E000: Seleciona o banco PRG que vai mapear em $C000 (bits 0-2)
            prgBank_C000 = data & 0x07;
            return true;
        }

        return false;
    }

    bool Mapper040::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        // O Mapper 40 utiliza CHR-RAM (8KB mapeados direto em $0000-$1FFF)
        if (addr <= 0x1FFF)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool Mapper040::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF)
        {
            // Emulação padrão: se chrBanks for 0, é indicativo de CHR-RAM.
            if (mChrBanks == 0)
            {
                mapped_addr = addr;
                return true;
            }
        }
        return false;
    }

    void Mapper040::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<char *>(&prgBank_C000), sizeof(prgBank_C000));
        os.write(reinterpret_cast<char *>(&bIRQEnabled), sizeof(bIRQEnabled));
        os.write(reinterpret_cast<char *>(&bIRQActive), sizeof(bIRQActive));
        os.write(reinterpret_cast<char *>(&irqCounter), sizeof(irqCounter));
    }

    void Mapper040::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&prgBank_C000), sizeof(prgBank_C000));
        is.read(reinterpret_cast<char *>(&bIRQEnabled), sizeof(bIRQEnabled));
        is.read(reinterpret_cast<char *>(&bIRQActive), sizeof(bIRQActive));
        is.read(reinterpret_cast<char *>(&irqCounter), sizeof(irqCounter));
    }

    bool Mapper040::getIrqFlag() const
    {
        return bIRQActive;
    }

    void Mapper040::clearIrqFlag()
    {
        bIRQActive = false;
    }

    void Mapper040::tick()
    {
        if (bIRQEnabled)
        {
            irqCounter++;

            // Dispara o IRQ na linha de scan 36 (Exatamente abaixo do status bar)
            if (irqCounter == 4096)
            {
                bIRQActive = true;
            }
            // Se o jogo falhar em reconhecer, o timer estoura e reseta
            else if (irqCounter == 8192)
            {
                bIRQActive = false;
                irqCounter = 0;
            }
        }
    }
}