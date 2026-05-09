#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper000.h"
#include "Core/Cartridge/Mappers/Mapper001.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace R2NES::Core
{
    Cartridge::Cartridge(const std::string &fileName)
    {
        // Estrutura do cabeçalho iNES (16 bytes)
        struct Header
        {
            char name[4];       // "NES" seguido de MS-DOS EOF
            uint8_t prg_chunks; // 16KB unidades
            uint8_t chr_chunks; // 8KB unidades
            uint8_t mapper1;    // Mirroring, battery, trainer, mapper lower nibble
            uint8_t mapper2;    // Mapper upper nibble, etc
            uint8_t prg_ram_size;
            uint8_t tv_system1;
            uint8_t tv_system2;
            char unused[5];
        } header;

        std::ifstream ifs;
        ifs.open(fileName, std::ifstream::binary);

        if (ifs.is_open())
        {
            ifs.read((char *)&header, sizeof(Header));

            // Valida a assinatura "NES" seguido do byte 0x1A
            if (std::memcmp(header.name, "NES\x1a", 4) != 0)
            {
                return;
            }

            // Se houver um "Trainer" (512 bytes antes da PRG ROM), ignoramos por enquanto
            if (header.mapper1 & 0x04)
            {
                ifs.seekg(512, std::ios_base::cur);
            }

            // Identifica o modo de espelhamento inicial do cabeçalho
            if (header.mapper1 & 0x08)
                mirror = MirrorMode::FOUR_SCREEN;
            else
                mirror = (header.mapper1 & 0x01) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;

            // Identifica o ID do Mapper
            mapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);

            // Lê PRG ROM
            prgBanks = header.prg_chunks;
            std::vector<uint8_t> prgData(prgBanks * 16384);
            ifs.read((char *)prgData.data(), prgData.size());
            prgROM = std::make_unique<PRGROM>(std::move(prgData));

            // Lê CHR ROM (ou prepara CHR RAM se o tamanho for 0)
            chrBanks = header.chr_chunks;
            if (chrBanks == 0)
            {
                std::vector<uint8_t> chrData(8192); // 8KB de CHR RAM padrão
                chrROM = std::make_unique<CHRROM>(std::move(chrData));
            }
            else
            {
                std::vector<uint8_t> chrData(chrBanks * 8192);
                ifs.read((char *)chrData.data(), chrData.size());
                chrROM = std::make_unique<CHRROM>(std::move(chrData));
            }

            // Instancia o Mapper correto
            switch (mapperID)
            {
            case 0:
                pMapper = std::make_shared<Mapper000>(prgBanks, chrBanks);
                break;
            case 1:
                pMapper = std::make_shared<Mapper001>(prgBanks, chrBanks);
                break;
            default:
                std::cerr << "Error: Mapper " << (int)mapperID << " is not supported yet. Only Mapper 0 (NROM) is available." << std::endl;
                return;
            }

            imageValid = true;
            ifs.close();
        }
    }

    bool Cartridge::cpuRead(uint16_t addr, uint8_t &data) const
    {
        uint32_t mapped_addr = 0;
        if (pMapper && pMapper->cpuMapRead(addr, mapped_addr, data))
        {
            if (mapped_addr == 0xFFFFFFFF) return true; // Dado já preenchido pelo Mapper (ex: PRG RAM)
            
            data = prgROM->read(mapped_addr);
            return true;
        }
        return false;
    }

    bool Cartridge::cpuWrite(uint16_t addr, uint8_t data)
    {
        uint32_t mapped_addr = 0;
        if (pMapper && pMapper->cpuMapWrite(addr, mapped_addr, data))
        {
            if (mapped_addr == 0xFFFFFFFF) return true; // Escrita tratada internamente pelo Mapper
            
            return true;
        }
        return false;
    }

    bool Cartridge::ppuRead(uint16_t addr, uint8_t &data) const
    {
        uint32_t mapped_addr = 0;
        if (pMapper && pMapper->ppuMapRead(addr, mapped_addr, data))
        {
            if (mapped_addr == 0xFFFFFFFF) return true;

            if (chrROM)
            {
                data = chrROM->read(mapped_addr);
                return true;
            }
        }
        return false;
    }

    bool Cartridge::ppuWrite(uint16_t addr, uint8_t data)
    {
        uint32_t mapped_addr = 0;
        if (pMapper && pMapper->ppuMapWrite(addr, mapped_addr, data))
        {
            if (mapped_addr == 0xFFFFFFFF) return true;

            chrROM->write(mapped_addr, data);
            return true;
        }
        return false;
    }

    MirrorMode Cartridge::getMirrorMode() const
    {
        // Mappers avançados (como MMC1) controlam o Mirroring via software.
        // Verificamos se o mapper atual suporta isso, caso contrário usamos o valor do cabeçalho.
        if (pMapper && (mapperID == 1)) 
        {
            return pMapper->getMirrorMode();
        }
        return mirror;
    }
}