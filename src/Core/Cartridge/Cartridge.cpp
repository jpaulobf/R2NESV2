#include "Core/Cartridge/Cartridge.h"
#include "Core/Cartridge/Mappers/Mapper000.h"
#include "Core/Cartridge/Mappers/Mapper001.h"
#include "Core/Cartridge/Mappers/Mapper002.h"
#include "Core/Cartridge/Mappers/Mapper003.h"
#include "Core/Cartridge/Mappers/Mapper004.h"
#include "Core/Cartridge/Mappers/Mapper005.h"
#include "Core/Cartridge/Mappers/Mapper007.h"
#include "Core/Cartridge/Mappers/Mapper009.h"
#include "Core/Cartridge/Mappers/Mapper011.h"
#include "Core/Cartridge/Mappers/Mapper023.h"
#include "Core/Cartridge/Mappers/Mapper040.h"
#include "Core/Cartridge/Mappers/Mapper066.h"
#include "Core/Cartridge/Mappers/Mapper071.h"
#include "Core/Cartridge/Mappers/Mapper090.h"
#include "Core/Cartridge/Mappers/Mapper187.h"
#include "Core/Cartridge/Mappers/Mapper189.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <zlib.h>

namespace R2NES::Core
{
    // NES 2.0: contagem linear de bancos ou tamanho em bytes 2^E * (2*M+1).
    static bool decodeRomSize(uint8_t lsb, uint8_t msb, uint32_t unit, size_t &size)
    {
        uint64_t bytes = 0;
        if (msb != 0x0F)
            bytes = (uint64_t(msb) * 256 + lsb) * unit;
        else
        {
            const uint64_t base = uint64_t(1) << (lsb >> 2);
            const uint64_t multiplier = (lsb & 3) * 2 + 1;
            if (base > std::numeric_limits<uint64_t>::max() / multiplier)
                return false;
            bytes = base * multiplier;
        }
        if (bytes > std::numeric_limits<size_t>::max())
            return false;
        size = static_cast<size_t>(bytes);
        return true;
    }

    // Função auxiliar para extrair arquivo .nes de um ZIP usando zlib
    static bool extractNESFromZIP(const std::string &zipFileName, std::vector<uint8_t> &buffer)
    {
        std::ifstream zipFile(zipFileName, std::ios::binary);
        if (!zipFile.is_open())
            return false;

        // Lê o arquivo ZIP completamente na memória
        zipFile.seekg(0, std::ios::end);
        std::streamsize zipSize = zipFile.tellg();
        zipFile.seekg(0, std::ios::beg);

        std::vector<uint8_t> zipData(zipSize);
        zipFile.read((char *)zipData.data(), zipSize);
        zipFile.close();

        // Procura pela assinatura do Local File Header (0x04034b50)
        uint32_t signature = 0x04034b50;
        size_t pos = 0;

        while (pos < zipData.size() - 30)
        {
            uint32_t header = *(uint32_t *)(zipData.data() + pos);
            if (header == signature)
            {
                // Lê o header do arquivo local
                uint16_t fileNameLength = *(uint16_t *)(zipData.data() + pos + 26);
                uint16_t extraFieldLength = *(uint16_t *)(zipData.data() + pos + 28);
                uint32_t compressedSize = *(uint32_t *)(zipData.data() + pos + 18);
                uint32_t uncompressedSize = *(uint32_t *)(zipData.data() + pos + 22);
                uint16_t compressionMethod = *(uint16_t *)(zipData.data() + pos + 8);

                // Lê o nome do arquivo
                std::string fileName((char *)(zipData.data() + pos + 30), fileNameLength);

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
                            std::memcpy(buffer.data(), zipData.data() + fileDataPos,
                                        uncompressedSize);
                            return true;
                        }
                        else if (compressionMethod == 8)
                        {
                            // Comprimido com DEFLATE - descomprimi com zlib
                            z_stream stream = {};
                            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                                return false;

                            stream.avail_in = compressedSize;
                            stream.next_in = (Bytef *)(zipData.data() + fileDataPos);

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
                std::cerr << "Error: Could not extract .nes file from ZIP " << fileName
                          << std::endl;
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
            std::cerr
                << "Error: Unsupported file format. Please use .nes or .zip files."
                << std::endl;
        }

        if (buffer.empty())
        {
            std::cerr << "Error: No ROM data read from " << fileName << std::endl;
        }
        else
        {
            imageValid = loadFromBuffer(buffer);
            if (imageValid)
            {
                romHash = calculateRomHash(buffer);
            }
        }
    }

    void Cartridge::clearIrqFlag()
    {
        pMapper->clearIrqFlag();
    }

    void Cartridge::tick()
    {
        if (pMapper)
        {
            pMapper->tick();
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

        if (buffer.size() < sizeof(Header))
        {
            std::cerr << "Error: ROM too small (" << buffer.size() << " bytes)." << std::endl;
            return false;
        }
        std::memcpy(&header, buffer.data(), sizeof(Header));

        if (std::memcmp(header.name, "NES\x1a", 4) != 0)
        {
            std::cerr << "Error: Invalid iNES header (missing 'NES\\x1A' magic)." << std::endl;
            return false;
        }

        size_t offset = sizeof(Header);
        if (header.mapper1 & 0x04)
            offset += 512;

        if (header.mapper1 & 0x08)
            mirror = MirrorMode::FOUR_SCREEN;
        else
            mirror =
                (header.mapper1 & 0x01) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;

        const bool isNes2Header = (header.mapper2 & 0x0C) == 0x08;
        const uint8_t submapper = isNes2Header ? header.prg_ram_size >> 4 : 0;
        mapperID = (header.mapper2 & 0xF0) | (header.mapper1 >> 4);
        if (isNes2Header)
            mapperID |= uint16_t(header.prg_ram_size & 0x0F) << 8;

        // This legacy SF II dump incorrectly declares mapper 187. Identify the
        // exact payload instead of changing the behavior of real mapper 187 ROMs.
        if (mapperID == 187 && header.prg_chunks == 8 && header.chr_chunks == 16 &&
            (header.mapper2 & 0x0C) != 0x08 && !(header.mapper1 & 0x04) &&
            calculateRomHash(buffer) == "96CE586E")
        {
            mapperID = 189;
            std::cout << "Cartridge: corrected legacy mapper 187 to 189 (CRC 96CE586E)."
                      << std::endl;
        }

        // UxROM: conflitos no iNES/submapper 0 e no submapper 2; nunca no 1.
        mapper2BusConflicts = !isNes2Header || submapper != 1;
        mapper3BusConflicts = !isNes2Header || submapper != 1;
        // Hayauchi Super Igo: NES 2.0 declara 2 KiB de PRG-RAM volátil.
        const uint32_t mapper3RamSize = isNes2Header && header.tv_system2 == 5 ? 2048 : 0;
        // AxROM: iNES e submappers 0/1 não aplicam conflitos (ANROM).
        // Somente NES 2.0 submapper 2 declara conflitos AND (AMROM/AOROM).
        mapper7BusConflicts = isNes2Header && (header.prg_ram_size >> 4) == 2;
        Mapper001::Configuration mmc1Config;
        mmc1Config.submapper = submapper;
        mmc1Config.hardwareMirror = mirror;
        if (isNes2Header)
        {
            const auto ramSize = [](uint8_t shift) -> uint32_t { return shift ? 64u << shift : 0; };
            mmc1Config.prgRamSize = ramSize(header.tv_system2 & 0x0F); // Byte 10.
            mmc1Config.prgNvRamSize = ramSize(header.tv_system2 >> 4);
        }
        else
        {
            // iNES sem tamanho explícito: mantém a suposição histórica de 8 KiB.
            const uint32_t ram = uint32_t(header.prg_ram_size ? header.prg_ram_size : 1) * 8192;
            mmc1Config.prgRamSize = (header.mapper1 & 2) ? 0 : ram;
            mmc1Config.prgNvRamSize = (header.mapper1 & 2) ? ram : 0;
        }

        Mapper004::Configuration mmc3Config;
        mmc3Config.submapper = submapper;
        mmc3Config.prgRamSize = mmc1Config.prgRamSize;
        mmc3Config.prgNvRamSize = mmc1Config.prgNvRamSize;

        size_t prgSize = size_t(header.prg_chunks) * 16384;
        size_t chrSize = size_t(header.chr_chunks) * 8192;
        if (isNes2Header &&
            (!decodeRomSize(header.prg_chunks, header.tv_system1 & 0x0F, 16384, prgSize) ||
             !decodeRomSize(header.chr_chunks, header.tv_system1 >> 4, 8192, chrSize)))
        {
            std::cerr << "Error: NES 2.0 ROM size overflows." << std::endl;
            return false;
        }
        // Os demais mappers ainda recebem contagens de oito bits nos construtores.
        const size_t maxPrgBanks = mapperID == 2 ? 256 : 255;
        if (prgSize == 0 || prgSize % 16384 != 0 || chrSize % 8192 != 0 ||
            prgSize / 16384 > maxPrgBanks || chrSize / 8192 > 255)
        {
            std::cerr << "Error: Unsupported PRG/CHR ROM size for mapper " << mapperID << "." << std::endl;
            return false;
        }
        prgBanks = static_cast<uint16_t>(prgSize / 16384);
        chrBanks = static_cast<uint16_t>(chrSize / 8192);
        if (mapperID == 1)
        {
            const uint8_t chrRam = static_cast<uint8_t>(header.unused[0]);
            if (!Mapper001::supports(prgBanks, chrBanks, mmc1Config) ||
                (isNes2Header && chrRam != (chrBanks == 0 ? 7 : 0)) ||
                ((header.mapper1 & 4) && mmc1Config.prgRamSize + mmc1Config.prgNvRamSize == 0))
            {
                std::cerr << "Error: Unsupported MMC1 memory/submapper configuration (submapper "
                          << unsigned(submapper) << ")." << std::endl;
                return false;
            }
        }
        if (mapperID == 2)
        {
            const uint8_t chrRam = static_cast<uint8_t>(header.unused[0]); // Byte 11 NES 2.0.
            if ((prgBanks & (prgBanks - 1)) != 0 || chrBanks > 1 ||
                mirror == MirrorMode::FOUR_SCREEN || (isNes2Header &&
                (submapper > 2 || header.tv_system2 != 0 ||
                 chrRam != (chrBanks == 0 ? 7 : 0))))
            {
                std::cerr << "Error: Unsupported Mapper 2 memory/submapper configuration." << std::endl;
                return false;
            }
        }
        if (mapperID == 3)
        {
            const uint8_t chrRam = static_cast<uint8_t>(header.unused[0]);
            if (!Mapper003::supports(prgBanks, chrBanks, mirror, mapper3RamSize) ||
                (header.mapper1 & 2) || // Não há suporte a NVRAM nesta placa.
                ((header.mapper1 & 4) && mapper3RamSize == 0) ||
                (isNes2Header && (submapper > 2 ||
                 (header.tv_system2 != 0 && header.tv_system2 != 5) ||
                 chrRam != (chrBanks == 0 ? 7 : 0) ||
                 (static_cast<uint8_t>(header.unused[3]) & 3) != 0)) ||
                (!isNes2Header && header.prg_ram_size != 0))
            {
                std::cerr << "Error: Unsupported Mapper 3 memory/submapper configuration." << std::endl;
                return false;
            }
        }
        if (mapperID == 7)
        {
            // AxROM não possui PRG-RAM, CHR-ROM, NVRAM ou RAM four-screen.
            // No iNES, o byte 8 é ambíguo e não seleciona um submapper.
            const uint8_t chrRam = static_cast<uint8_t>(header.unused[0]);
            if (!Mapper007::supports(prgBanks, chrBanks, mirror) ||
                (header.mapper1 & 6) || // Bateria e trainer exigiriam memória ausente.
                (isNes2Header && (submapper > 2 || header.tv_system2 != 0 || chrRam != 7 ||
                 (static_cast<uint8_t>(header.unused[3]) & 3) != 0)))
            {
                std::cerr << "Error: Unsupported Mapper 7 memory/submapper configuration." << std::endl;
                return false;
            }
        }
        if (mapperID == 9)
        {
            // MMC2/PxROM: apenas PRG-ROM e CHR-ROM, sem RAM ou variantes declaradas.
            if (!Mapper009::supports(prgBanks, chrBanks, mirror) || (header.mapper1 & 6) ||
                (isNes2Header && (submapper != 0 || header.tv_system2 != 0 ||
                 static_cast<uint8_t>(header.unused[0]) != 0 ||
                 (static_cast<uint8_t>(header.unused[3]) & 3) != 0)))
            {
                std::cerr << "Error: Unsupported Mapper 9 memory/submapper configuration." << std::endl;
                return false;
            }
        }
        if (mapperID == 4)
        {
            // Não basta reconhecer o número do mapper: submapper, RAM e
            // dados adicionais do header precisam corresponder à placa emulada.
            const uint8_t chrRam = static_cast<uint8_t>(header.unused[0]);
            if (!Mapper004::supports(prgBanks, chrBanks, mirror, mmc3Config) ||
                (isNes2Header && (chrRam != (chrBanks == 0 ? 7 : 0) ||
                 (static_cast<uint8_t>(header.unused[3]) & 3) != 0)) ||
                ((header.mapper1 & 4) && mmc3Config.prgRamSize + mmc3Config.prgNvRamSize == 0))
            {
                std::cerr << "Error: Unsupported Mapper 4 memory/submapper configuration." << std::endl;
                return false;
            }
        }
        // Os derivados compartilham o construtor MMC3 validado. A placa 187
        // acrescenta uma linha CHR; rejeite headers inválidos antes de construir.
        if (mapperID == 187 || mapperID == 189)
        {
            Mapper004::Configuration derivedConfig;
            derivedConfig.extendedChr = mapperID == 187;
            if (!Mapper004::supports(prgBanks, chrBanks, mirror, derivedConfig))
            {
                std::cerr << "Error: Unsupported memory configuration for mapper " << mapperID << "." << std::endl;
                return false;
            }
        }
        if (offset > buffer.size())
        {
            std::cerr << "Error: Truncated trainer." << std::endl;
            return false;
        }
        if (prgSize > buffer.size() - offset)
        {
            if (mapperID == 1 || mapperID == 2 || mapperID == 3 || mapperID == 4 || mapperID == 7 || mapperID == 9 ||
                mapperID == 187 || mapperID == 189 || isNes2Header)
            {
                std::cerr << "Error: Truncated PRG ROM." << std::endl;
                return false;
            }
            size_t available = (buffer.size() > offset) ? buffer.size() - offset : 0;
            size_t fitBanks = available / 16384;
            if (fitBanks == 0)
            {
                std::cerr << "Error: PRG data exceeds file size (PRG banks=" << (int)prgBanks
                          << ", file=" << buffer.size() << " bytes)." << std::endl;
                return false;
            }
            std::cerr << "Warning: header declares " << (int)prgBanks << " PRG banks but file only has "
                      << fitBanks << "; using " << fitBanks << "." << std::endl;
            prgBanks = static_cast<decltype(prgBanks)>(fitBanks);
            prgSize = fitBanks * 16384;
        }

        std::vector<uint8_t> prgData(prgSize);
        std::memcpy(prgData.data(), buffer.data() + offset, prgSize);
        prgROM = std::make_unique<PRGROM>(std::move(prgData));
        offset += prgSize;

        if (chrBanks == 0)
        {
            std::vector<uint8_t> chrData(8192);
            chrROM = std::make_unique<CHRROM>(std::move(chrData));
        }
        else
        {
            if (chrSize > buffer.size() - offset)
            {
                if (mapperID == 1 || mapperID == 2 || mapperID == 3 || mapperID == 4 || mapperID == 9 ||
                    mapperID == 187 || mapperID == 189 || isNes2Header)
                {
                    std::cerr << "Error: Truncated CHR ROM." << std::endl;
                    return false;
                }
                size_t available = (buffer.size() > offset) ? buffer.size() - offset : 0;
                size_t fitBanks = available / 8192;
                if (fitBanks == 0)
                {
                    std::cerr << "Error: CHR data exceeds file size (CHR banks=" << (int)chrBanks
                              << ", file=" << buffer.size() << " bytes)." << std::endl;
                    return false;
                }
                std::cerr << "Warning: header declares " << (int)chrBanks << " CHR banks but file only has "
                          << fitBanks << "; using " << fitBanks << "." << std::endl;
                chrBanks = static_cast<decltype(chrBanks)>(fitBanks);
                chrSize = fitBanks * 8192;
            }
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
            pMapper = std::make_shared<Mapper001>(prgBanks, chrBanks, mmc1Config);
            if (header.mapper1 & 4)
            {
                for (uint16_t index = 0; index < 512; ++index)
                    cpuWrite(0x7000 + index, buffer[sizeof(Header) + index], 0);
            }
            break;
        case 2:
            pMapper = std::make_shared<Mapper002>(prgBanks, chrBanks, mirror);
            break;
        case 3:
            pMapper = std::make_shared<Mapper003>(prgBanks, chrBanks, mirror, mapper3RamSize);
            if (header.mapper1 & 4)
            {
                for (uint16_t index = 0; index < 512; ++index)
                    cpuWrite(0x7000 + index, buffer[sizeof(Header) + index], 0);
            }
            break;
        case 4:
            pMapper = std::make_shared<Mapper004>(prgBanks, chrBanks, mirror, mmc3Config);
            if (header.mapper1 & 4)
                std::static_pointer_cast<Mapper004>(pMapper)->loadTrainer(buffer.data() + sizeof(Header));
            break;
        case 5:
            pMapper = std::make_shared<Mapper005>(prgBanks, chrBanks);
            break;
        case 7:
            pMapper = std::make_shared<Mapper007>(prgBanks, chrBanks, mirror);
            break;
        case 9:
            pMapper = std::make_shared<Mapper009>(prgBanks, chrBanks, mirror);
            break;
        case 11:
            pMapper = std::make_shared<Mapper011>(prgBanks, chrBanks, mirror);
            break;
        case 23:
            pMapper = std::make_shared<Mapper023>(prgBanks, chrBanks, mirror);
            break;
        case 40:
            pMapper = std::make_shared<Mapper040>(prgBanks, chrBanks);
            break;
        case 66:
            pMapper = std::make_shared<Mapper066>(prgBanks, chrBanks, mirror);
            break;
        case 71:
            pMapper = std::make_shared<Mapper071>(
                prgBanks, chrBanks, mirror,
                isNes2Header ? header.prg_ram_size >> 4 : 0, !isNes2Header);
            break;
        case 90:
            pMapper = std::make_shared<Mapper090>(prgBanks, chrBanks);
            break;
        case 187:
            pMapper = std::make_shared<Mapper187>(prgBanks, chrBanks, mirror);
            break;
        case 189:
            pMapper = std::make_shared<Mapper189>(prgBanks, chrBanks, mirror);
            break;
        default:
            std::cerr << "Error: Mapper " << (int)mapperID << " is not supported yet."
                      << std::endl;
            return false;
        }

        return true;
    }

    bool Cartridge::cpuRead(uint16_t addr, uint8_t &data) const
    {
        uint32_t mapped_addr = 0;
        if (pMapper && pMapper->cpuMapRead(addr, mapped_addr, data))
        {
            if (mapped_addr == 0xFFFFFFFF)
                return true; // Dado já preenchido pelo Mapper (ex: PRG RAM)

            data = prgROM->read(mapped_addr);
            return true;
        }
        return false;
    }

    bool Cartridge::cpuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter,
                             std::optional<uint32_t> cpuCycleCounter)
    {
        uint32_t mapped_addr = 0;
        if (pMapper)
        {
            // Simulação de conflitos de barramento (Bus Conflicts) em mappers discretos.
            // No NES real, a ROM não é desabilitada durante a escrita, fazendo com que o valor
            // escrito seja o AND lógico entre o valor da CPU e o valor armazenado na ROM naquele endereço.
            if (addr >= 0x8000 && addr <= 0xFFFF)
            {
                if ((mapperID == 2 && mapper2BusConflicts) || (mapperID == 3 && mapper3BusConflicts) ||
                    (mapperID == 7 && mapper7BusConflicts) || mapperID == 66)
                {
                    uint32_t read_mapped_addr = 0;
                    uint8_t dummy_data = 0;
                    if (pMapper->cpuMapRead(addr, read_mapped_addr, dummy_data))
                    {
                        if (read_mapped_addr != 0xFFFFFFFF && read_mapped_addr < prgROM->size())
                        {
                            data &= prgROM->read(read_mapped_addr);
                        }
                    }
                }
            }

            const uint32_t mapperClock = mapperID == 1 && cpuCycleCounter ? *cpuCycleCounter : systemClockCounter;
            if (pMapper->cpuMapWrite(addr, mapped_addr, data, mapperClock))
            {
                if (mapped_addr == 0xFFFFFFFF)
                    return true; // Escrita tratada internamente pelo Mapper

                return true;
            }
        }
        return false;
    }

    bool Cartridge::ppuRead(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const
    {
        uint32_t mapped_addr = 0;
        if (pMapper &&
            pMapper->ppuMapRead(addr, mapped_addr, data, systemClockCounter))
        {
            if (mapped_addr == 0xFFFFFFFF)
                return true;

            if (chrROM)
            {
                data = chrROM->read(mapped_addr);
                return true;
            }
        }
        return false;
    }

    bool Cartridge::ppuPeek(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const
    {
        uint32_t mapped_addr = 0;
        if (!pMapper || !pMapper->ppuMapPeek(addr, mapped_addr, data, systemClockCounter))
            return false;
        if (mapped_addr == 0xFFFFFFFF)
            return true;
        if (!chrROM)
            return false;
        data = chrROM->read(mapped_addr);
        return true;
    }

    bool Cartridge::ppuReadSprite(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const
    {
        if (!pMapper)
            return false;

        pMapper->setPpuReadIsSprite(true);
        bool handled = ppuRead(addr, data, systemClockCounter);
        pMapper->setPpuReadIsSprite(false);
        return handled;
    }

    void Cartridge::ppuAddressUpdated(uint16_t addr, uint32_t systemClockCounter)
    {
        if (pMapper)
            pMapper->onPpuAddress(addr, systemClockCounter);
    }

    void Cartridge::ppuScanlineStart()
    {
        if (pMapper)
            pMapper->onPpuScanlineStart();
    }

    bool Cartridge::ppuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter)
    {
        uint32_t mapped_addr = 0;
        if (pMapper &&
            pMapper->ppuMapWrite(addr, mapped_addr, data, systemClockCounter))
        {
            if (mapped_addr == 0xFFFFFFFF)
                return true;

            chrROM->write(mapped_addr, data);
            return true;
        }
        return false;
    }

    void Cartridge::saveState(std::ostream &os) const
    {
        if (chrBanks == 0 && chrROM)
            chrROM->saveState(os);
    }

    void Cartridge::loadState(std::istream &is)
    {
        if (chrBanks == 0 && chrROM)
            chrROM->loadState(is);
    }

    MirrorMode Cartridge::getMirrorMode() const
    {
        // Mappers avançados (como MMC1) controlam o Mirroring via software.
        // Verificamos se o Mapper implementa getMirrorMode
        if (pMapper &&
            (mapperID == 1 || mapperID == 4 || mapperID == 5 || mapperID == 7 || mapperID == 9 ||
             mapperID == 11 || mapperID == 19 || mapperID == 21 || mapperID == 22 ||
             mapperID == 23 || mapperID == 24 || mapperID == 26 || mapperID == 71 || mapperID == 85 || mapperID == 187 || mapperID == 189))
        {
            return pMapper->getMirrorMode();
        }
        return mirror;
    }

    bool Cartridge::getIrqFlag() const
    {
        if (pMapper)
            return pMapper->getIrqFlag();
        return false;
    }

    std::string Cartridge::calculateRomHash(const std::vector<uint8_t> &buffer)
    {
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, buffer.data() + 16, buffer.size() - 16);
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << crc;
        return ss.str();
    }
}
