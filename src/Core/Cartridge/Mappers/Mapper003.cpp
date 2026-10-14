#include "Core/Cartridge/Mappers/Mapper003.h"
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper003::supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, uint32_t prgRamSize)
    {
        return (prgBanks == 1 || prgBanks == 2) &&
               chrBanks <= 16 && (chrBanks == 0 || (chrBanks & (chrBanks - 1)) == 0) &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL) &&
               (prgRamSize == 0 || prgRamSize == 2048);
    }

    Mapper003::Mapper003(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, uint32_t prgRamSize)
        : Mapper(prgBanks, chrBanks)
    {
        if (!supports(prgBanks, chrBanks, mirror, prgRamSize))
            throw std::invalid_argument("Unsupported Mapper 3 memory configuration");
        prgRAM.resize(prgRamSize);
        latchMask = chrBanks > 4 ? 0x0F : 0x03;
        mirrorMode = mirror;
        reset();
    }

    Mapper003::~Mapper003() {}

    bool Mapper003::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF && !prgRAM.empty())
        {
            data = prgRAM[addr & 0x07FF];
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // PRG ROM: 16KB (espelhado) ou 32KB fixo, igual ao Mapper 000
            mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool Mapper003::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF && !prgRAM.empty())
        {
            prgRAM[addr & 0x07FF] = data;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // CNROM comum usa dois bits; a variante oversized usa quatro.
            // Preserva o latch, mesmo quando algumas linhas da ROM não estão conectadas.
            nCHRBankSelect = data & latchMask;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }

        return false;
    }

    bool Mapper003::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            // Mapeia o banco de 8KB selecionado
            const uint8_t bank = nCHRBanks ? nCHRBankSelect & (nCHRBanks - 1) : 0;
            mapped_addr = bank * 0x2000 + addr;
            return true;
        }
        return false;
    }

    bool Mapper003::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr <= 0x1FFF && nCHRBanks == 0)
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    MirrorMode Mapper003::getMirrorMode()
    {
        // Retorna o modo definido no hardware (passado via constructor)
        return mirrorMode;
    }

    void Mapper003::reset()
    {
        // Inicialização completa e determinística do emulador, não reset elétrico.
        // A RAM do cartucho permanece intacta.
        nCHRBankSelect = 0;
    }

    void Mapper003::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
        // Preserva os cinco bytes dos estados antigos no Windows.
        const uint32_t mirror = static_cast<uint32_t>(mirrorMode);
        for (unsigned shift = 0; shift < 32; shift += 8)
            os.put(static_cast<char>((mirror >> shift) & 0xFF));
        if (!prgRAM.empty())
            os.write(reinterpret_cast<const char *>(prgRAM.data()), prgRAM.size());
    }

    void Mapper003::loadState(std::istream &is)
    {
        uint8_t bank = 0;
        uint32_t mirror = 0;
        is.read(reinterpret_cast<char *>(&bank), sizeof(bank));
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            const int byte = is.get();
            if (byte == std::char_traits<char>::eof())
                return;
            mirror |= uint32_t(static_cast<uint8_t>(byte)) << shift;
        }
        auto ram = prgRAM;
        if (!ram.empty())
            is.read(reinterpret_cast<char *>(ram.data()), ram.size());
        if (!is || bank > latchMask || mirror != static_cast<uint32_t>(mirrorMode))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        nCHRBankSelect = bank;
        prgRAM = std::move(ram);
    }
}
