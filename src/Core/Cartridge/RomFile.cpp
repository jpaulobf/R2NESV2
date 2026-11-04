#include "Core/Cartridge/RomFile.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <zlib.h>

namespace R2NES::Core
{
    namespace
    {
        // Verifica se o intervalo [p, p + n) cabe no vetor, sem somar antes da validação.
        bool has(const std::vector<uint8_t> &v, size_t p, size_t n)
        {
            return p <= v.size() && n <= v.size() - p;
        }

        // Lê um inteiro de 16 bits em ordem little-endian; o intervalo deve ser válido.
        uint16_t u16(const std::vector<uint8_t> &v, size_t p)
        {
            return uint16_t(v[p]) | (uint16_t(v[p + 1]) << 8);
        }

        // Lê um inteiro de 32 bits em ordem little-endian; o intervalo deve ser válido.
        uint32_t u32(const std::vector<uint8_t> &v, size_t p)
        {
            return uint32_t(u16(v, p)) | (uint32_t(u16(v, p + 2)) << 16);
        }

        // Identifica extensões de ROM aceitas, sem diferenciar maiúsculas de minúsculas.
        bool isRom(const std::string &name)
        {
            const auto dot = name.find_last_of('.');
            if (dot == std::string::npos)
                return false;
            std::string ext = name.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c)
                           { return char(std::tolower(c)); });
            return ext == ".nes" || ext == ".unf" || ext == ".unif";
        }
    }

    bool readRomFile(const std::string &path, size_t limit, std::vector<uint8_t> &data)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in)
            return false;
        const auto end = in.tellg();
        if (end < 0 || static_cast<uint64_t>(end) > limit)
            return false;
        std::vector<uint8_t> bytes(static_cast<size_t>(end));
        in.seekg(0);
        if (!in || (!bytes.empty() && !in.read(reinterpret_cast<char *>(bytes.data()), bytes.size())))
            return false;
        data = std::move(bytes);
        return true;
    }

    bool extractRomFromZip(const std::vector<uint8_t> &zip, std::vector<uint8_t> &rom)
    {
        // O diretório central fornece os metadados, inclusive quando há descritor de dados.
        if (zip.size() < 22 || zip.size() > MaxZipFileSize)
            return false;

        // Procura o fim do diretório central dentro do tamanho máximo do comentário ZIP.
        const size_t first = zip.size() > 65557 ? zip.size() - 65557 : 0;
        size_t end = zip.size() - 22;
        while (!(u32(zip, end) == 0x06054b50 && has(zip, end, 22) &&
                 u16(zip, end + 20) == zip.size() - end - 22))
        {
            if (end == first)
                return false;
            --end;
        }
        const uint16_t count = u16(zip, end + 10);
        const size_t cdSize = u32(zip, end + 12), cdOffset = u32(zip, end + 16);
        if (u16(zip, end + 4) || u16(zip, end + 6) || u16(zip, end + 8) != count ||
            count == 0xffff || cdOffset > end || cdSize != end - cdOffset)
            return false; // Arquivos divididos em discos e ZIP64 não são aceitos.

        size_t p = cdOffset;
        std::vector<uint8_t> selected;
        bool found = false;
        
        // Percorre todo o diretório, mesmo após encontrar a primeira ROM.
        for (unsigned i = 0; i < count; ++i)
        {
            if (!has(zip, p, 46) || p + 46 > end || u32(zip, p) != 0x02014b50)
                return false;
            const uint16_t flags = u16(zip, p + 8), method = u16(zip, p + 10);
            const uint32_t crc = u32(zip, p + 16), compressed = u32(zip, p + 20), unpacked = u32(zip, p + 24);
            const size_t nameLen = u16(zip, p + 28), extra = u16(zip, p + 30), comment = u16(zip, p + 32);
            const size_t local = u32(zip, p + 42), length = 46 + nameLen + extra + comment;
            if (length > end - p || u16(zip, p + 34) != 0)
                return false;
            const std::string name(reinterpret_cast<const char *>(zip.data() + p + 46), nameLen);
            if (!found && isRom(name))
            {
                // Aceita dados armazenados ou DEFLATE bruto e rejeita flags não suportadas.
                if ((flags & ~uint16_t(0x080e)) || (method != 0 && method != 8) ||
                    (method == 0 && (flags & 6)) || !unpacked || unpacked > MaxRomFileSize ||
                    compressed > MaxZipFileSize || !has(zip, local, 30) || local >= cdOffset ||
                    u32(zip, local) != 0x04034b50 || u16(zip, local + 6) != flags ||
                    u16(zip, local + 8) != method)
                    return false;
                // Confere o cabeçalho local e o nome com os dados do diretório central.
                const size_t localName = u16(zip, local + 26), localExtra = u16(zip, local + 28);
                if (localName != nameLen || local + 30 > cdOffset ||
                    localName + localExtra > cdOffset - local - 30 ||
                    std::memcmp(zip.data() + local + 30, name.data(), nameLen) != 0)
                    return false;
                const size_t data = local + 30 + localName + localExtra;
                if (compressed > cdOffset - data)
                    return false;
                if (!(flags & 8) && (u32(zip, local + 14) != crc || u32(zip, local + 18) != compressed ||
                                     u32(zip, local + 22) != unpacked))
                    return false;
                std::vector<uint8_t> result(unpacked);
                if (method == 0)
                {
                    if (compressed != unpacked)
                        return false;
                    std::memcpy(result.data(), zip.data() + data, unpacked);
                }
                else
                {
                    // O ZIP contém um fluxo DEFLATE bruto, sem cabeçalho zlib.
                    z_stream stream{};
                    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                        return false;
                    stream.next_in = const_cast<Bytef *>(zip.data() + data);
                    stream.avail_in = compressed;
                    stream.next_out = result.data();
                    stream.avail_out = unpacked;
                    const int status = inflate(&stream, Z_FINISH);
                    const bool ok = status == Z_STREAM_END && stream.total_in == compressed && stream.total_out == unpacked;
                    inflateEnd(&stream);
                    if (!ok)
                        return false;
                }
                // Confirma a integridade dos dados descompactados.
                if (crc32(0, result.data(), unpacked) != crc)
                    return false;
                selected = std::move(result);
                found = true;
            }
            p += length;
        }
        if (!found || p != end)
            return false;
        rom = std::move(selected);
        return true;
    }
}