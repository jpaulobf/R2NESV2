#include "Core/Cartridge/Mappers/Mapper066.h"
#include <iostream>

namespace R2NES::Core
{
    Mapper066::Mapper066(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror)
        : Mapper(prgBanks, chrBanks)
    {
        // Usamos std::dec para garantir que os tamanhos não sejam impressos em Hexadecimal
        std::cout << "Mapper66: Initialized. PRG Size: " << std::dec << (int)prgBanks * 16 << "KB | CHR Size: " << (int)chrBanks * 8 << "KB" << std::endl;
        mirrorMode = mirror;
        reset();
    }

    Mapper066::~Mapper066()
    {
    }

    bool Mapper066::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // GxROM (Mapper 66) chaveia janelas fixas de 32KB.
            // nPRGBanks é a contagem de chunks de 16KB (padrão iNES).
            uint32_t offset = addr & 0x7FFF;
            uint32_t totalSize = static_cast<uint32_t>(nPRGBanks) * 16384;
            mapped_addr = (static_cast<uint32_t>(nPRGBankSelect) * 0x8000 + offset) % totalSize;
            return true;
        }
        return false;
    }

    bool Mapper066::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint8_t oldPRG = nPRGBankSelect;
            uint8_t oldCHR = nCHRBankSelect;

            // Formato: [..CC..PP]
            // PRG: Bancos de 32KB. Como nPRGBanks está em 16KB, dividimos por 2.
            uint8_t nPRG32kChunks = nPRGBanks / 2;
            if (nPRG32kChunks > 0)
                nPRGBankSelect = (data & 0x03) % nPRG32kChunks;
            else
                nPRGBankSelect = 0;

            // CHR: Bancos de 8KB.
            if (nCHRBanks > 0)
                nCHRBankSelect = ((data >> 4) & 0x03) % nCHRBanks;
            else
                nCHRBankSelect = 0;

            if (nPRGBankSelect != oldPRG || nCHRBankSelect != oldCHR)
            {
                // Nota: addr aqui é o endereço da escrita. Usamos um valor arbitrário ou passamos o PC se quiser.
                std::cout << "Mapper66: Bank Switch! PRG:" << (int)oldPRG << "->" << (int)nPRGBankSelect << " | CHR:" << (int)oldCHR << "->" << (int)nCHRBankSelect << std::endl;
            }

            return true; // Importante: Indica que o mapper tratou a escrita
        }

        return false;
    }

    bool Mapper066::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            if (nCHRBanks == 0) 
            {
                // Caso o mapper seja forçado em uma ROM com CHR-RAM
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

    bool Mapper066::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
        {
            // Permite escrita se estivermos usando CHR-RAM
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    MirrorMode Mapper066::getMirrorMode()
    {
        return mirrorMode;
    }

    void Mapper066::reset()
    {
        // Para o Mapper 066 (GxROM), o sistema deve sempre iniciar no Banco 0.
        // Quase todos os jogos deste mapper esperam o boot no início da ROM.
        nPRGBankSelect = 0;
        nCHRBankSelect = 0;
    }

    void Mapper066::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
    }

    void Mapper066::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&nCHRBankSelect), sizeof(nCHRBankSelect));
    }
}