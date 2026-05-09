#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper000.h"
#include "Core/Cartridge/Mappers/Mapper001.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <zlib.h>
#include <algorithm>

namespace R2NES::Core
{
    // Função auxiliar para extrair arquivo .nes de um ZIP usando zlib
    static bool extractNESFromZIP(const std::string &zipFileName, std::vector<uint8_t> &buffer)
    {
        std::ifstream zipFile(zipFileName, std::ios::binary);
        if (!zipFile.is_open()) return false;

        // Lê o arquivo ZIP completamente na memória
        zipFile.seekg(0, std::ios::end);
        std::streamsize zipSize = zipFile.tellg();
        zipFile.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> zipData(zipSize);
        zipFile.read((char*)zipData.data(), zipSize);
        zipFile.close();

        // Procura pela assinatura do Local File Header (0x04034b50)
        uint32_t signature = 0x04034b50;
        size_t pos = 0;

        while (pos < zipData.size() - 30)
        {
            uint32_t header = *(uint32_t*)(zipData.data() + pos);
            if (header == signature)
            {
                // Lê o header do arquivo local
                uint16_t fileNameLength = *(uint16_t*)(zipData.data() + pos + 26);
                uint16_t extraFieldLength = *(uint16_t*)(zipData.data() + pos + 28);
                uint32_t compressedSize = *(uint32_t*)(zipData.data() + pos + 18);
                uint32_t uncompressedSize = *(uint32_t*)(zipData.data() + pos + 22);
                uint16_t compressionMethod = *(uint16_t*)(zipData.data() + pos + 8);

                // Lê o nome do arquivo
                std::string fileName((char*)(zipData.data() + pos + 30), fileNameLength);

                // Verifica se é um arquivo .nes
                if (fileName.size() > 4)
                {
                    std::string ext = fileName.substr(fileName.size() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".nes")
                    {
                        size_t fileDataPos = pos + 30 + fileNameLength + extraFieldLength;

                        if (compressionMethod == 0)
                        {
                            // Sem compressão - copia direto
                            buffer.resize(uncompressedSize);
                            std::memcpy(buffer.data(), zipData.data() + fileDataPos, uncompressedSize);
                            return true;
                        }
                        else if (compressionMethod == 8)
                        {
                            // Comprimido com DEFLATE - descomprimi com zlib
                            z_stream stream = {};
                            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                                return false;

                            stream.avail_in = compressedSize;
                            stream.next_in = (Bytef*)(zipData.data() + fileDataPos);

                            buffer.resize(uncompressedSize);
                            stream.avail_out = uncompressedSize;
                            stream.next_out = buffer.data();

                            int ret = inflate(&stream, Z_FINISH);
                            inflateEnd(&stream);

                            if (ret == Z_STREAM_END)
                                return true;
                            else
                                return false;
                        }
                    }
                }

                pos += 30 + fileNameLength + extraFieldLength + compressedSize;
            }
            else
            {
                pos++;
            }
        }

        return false;
    }

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
            // Tenta extrair arquivo .nes do ZIP usando zlib
            if (!extractNESFromZIP(fileName, buffer))
            {
                std::cerr << "Error: Could not extract .nes file from ZIP " << fileName << std::endl;
            }
        }
        else if (ext == ".nes")
        {
            // Lê arquivo .nes diretamente
            std::ifstream ifs(fileName, std::ifstream::binary | std::ifstream::ate);
            if (ifs.is_open())
            {
                std::streamsize size = ifs.tellg();
                ifs.seekg(0, std::ios::beg);
                buffer.resize(size);
                ifs.read((char *)buffer.data(), size);
                ifs.close();
            }
            else
            {
                std::cerr << "Error: Could not open file " << fileName << std::endl;
            }
        }
        else
        {
            std::cerr << "Error: Unsupported file format. Please use .nes or .zip files." << std::endl;
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