#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper000.h"
#include "Core/Cartridge/Mappers/Mapper001.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <zip.h>
#include <algorithm>

namespace R2NES::Core
{
    Cartridge::Cartridge(const std::string &fileName)
    {
        std::vector<uint8_t> buffer;
        std::string ext = "";
        size_t dotPos = fileName.find_last_of(".");
        if (dotPos != std::string::npos)
            ext = fileName.substr(dotPos);

        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".zip")
        {
            int err = 0;
            zip_t *archive = zip_open(fileName.c_str(), ZIP_RDONLY, &err);
            if (archive)
            {
                zip_int64_t num_entries = zip_get_num_entries(archive, 0);
                for (zip_int64_t i = 0; i < num_entries; i++)
                {
                    std::string name = zip_get_name(archive, i, 0);
                    if (name.size() > 4 && (name.substr(name.size() - 4) == ".nes" || name.substr(name.size() - 4) == ".NES"))
                    {
                        zip_stat_t st;
                        zip_stat_init(&st);
                        zip_stat_index(archive, i, 0, &st);

                        buffer.resize(st.size);
                        zip_file_t *f = zip_fopen_index(archive, i, 0);
                        if (f)
                        {
                            zip_fread(f, buffer.data(), st.size);
                            zip_fclose(f);
                        }
                        break;
                    }
                }
                zip_close(archive);
            }
        }
        else
        {
            std::ifstream ifs(fileName, std::ifstream::binary | std::ifstream::ate);
            if (ifs.is_open())
            {
                std::streamsize size = ifs.tellg();
                ifs.seekg(0, std::ios::beg);
                buffer.resize(size);
                ifs.read((char *)buffer.data(), size);
                ifs.close();
            }
        }

        if (!buffer.empty())
        {
            imageValid = loadFromBuffer(buffer);
        }
    }

    bool Cartridge::loadFromBuffer(const std::vector<uint8_t> &buffer)
    {
        struct Header
        {
            char name[4];
            uint8_t prg_chunks;
            uint8_t chr_chunks;
            uint8_t mapper1;
            uint8_t mapper2;
            uint8_t prg_ram_size;
            uint8_t tv_system1;
            uint8_t tv_system2;
            char unused[5];
        } header;

        if (buffer.size() < sizeof(Header)) return false;
        std::memcpy(&header, buffer.data(), sizeof(Header));

        if (std::memcmp(header.name, "NES\x1a", 4) != 0) return false;

        size_t offset = sizeof(Header);
        if (header.mapper1 & 0x04) offset += 512;

        if (header.mapper1 & 0x08)
            mirror = MirrorMode::FOUR_SCREEN;
        else
            mirror = (header.mapper1 & 0x01) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;

        mapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);

        prgBanks = header.prg_chunks;
        size_t prgSize = prgBanks * 16384;
        if (offset + prgSize > buffer.size()) return false;
        
        std::vector<uint8_t> prgData(prgSize);
        std::memcpy(prgData.data(), buffer.data() + offset, prgSize);
        prgROM = std::make_unique<PRGROM>(std::move(prgData));
        offset += prgSize;

        chrBanks = header.chr_chunks;
        if (chrBanks == 0)
        {
            std::vector<uint8_t> chrData(8192);
            chrROM = std::make_unique<CHRROM>(std::move(chrData));
        }
        else
        {
            size_t chrSize = chrBanks * 8192;
            if (offset + chrSize > buffer.size()) return false;
            std::vector<uint8_t> chrData(chrSize);
            std::memcpy(chrData.data(), buffer.data() + offset, chrSize);
            chrROM = std::make_unique<CHRROM>(std::move(chrData));
        }

        switch (mapperID)
        {
        case 0:
            pMapper = std::make_shared<Mapper000>(prgBanks, chrBanks);
            break;
        case 1:
            pMapper = std::make_shared<Mapper001>(prgBanks, chrBanks);
            break;
        default:
            std::cerr << "Error: Mapper " << (int)mapperID << " is not supported yet." << std::endl;
            return false;
        }

        return true;
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