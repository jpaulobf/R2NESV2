#include "Core/Cartridge/Mappers/Mapper011.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper011::supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
    {
        // Quatro bancos PRG de 32 KiB e dezesseis bancos CHR de 8 KiB.
        // Mantém as extensões anteriores: PRG de 16 KiB espelhada e CHR-RAM fixa.
        return prgBanks >= 1 && prgBanks <= 8 && (prgBanks & (prgBanks - 1)) == 0 &&
               chrBanks <= 16 && (chrBanks == 0 || (chrBanks & (chrBanks - 1)) == 0) &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper011::Mapper011(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
        : Mapper(prgBanks, chrBanks), mirrorMode(mirror)
    {
        if (!supports(prgBanks, chrBanks, mirror))
            throw std::invalid_argument("Unsupported Mapper 11 memory configuration");
        reset();
    }

    Mapper011::~Mapper011() {}

    bool Mapper011::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Linhas de endereço ausentes espelham a ROM física, inclusive
            // a extensão de 16 KiB nas duas metades da janela de 32 KiB.
            const uint32_t totalSize = static_cast<uint32_t>(nPRGBanks) * 16384;
            mapped_addr = (static_cast<uint32_t>(nPRGBankSelect) * 0x8000 + (addr & 0x7FFF)) & (totalSize - 1);
            return true;
        }
        return false;
    }

    bool Mapper011::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // CCCC LLPP: PP seleciona PRG, CCCC seleciona CHR. LL controla
            // o circuito de bloqueio CIC, que não precisa ser emulado aqui.
            // Quando a placa possui conflitos, o cartucho aplica o AND antes daqui.
            const uint8_t prgMask = nPRGBanks >= 2 ? nPRGBanks / 2 - 1 : 0;
            nPRGBankSelect = data & prgMask;
            nCHRBankSelect = nCHRBanks ? (data >> 4) & (nCHRBanks - 1) : 0;
            mapped_addr = 0xFFFFFFFF; // Registrador atendido; nunca grava PRG-ROM.
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

            mapped_addr = static_cast<uint32_t>(nCHRBankSelect) * 0x2000 + addr;
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
        uint8_t prg = 0, chr = 0;
        is.read(reinterpret_cast<char *>(&prg), sizeof(prg));
        is.read(reinterpret_cast<char *>(&chr), sizeof(chr));
        const unsigned prgCount = nPRGBanks >= 2 ? nPRGBanks / 2 : 1;
        const unsigned chrCount = nCHRBanks ? nCHRBanks : 1;
        if (!is || prg >= prgCount || chr >= chrCount)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        // Conserva os dois bytes históricos e só aplica um estado completo válido.
        nPRGBankSelect = prg;
        nCHRBankSelect = chr;
    }
}
