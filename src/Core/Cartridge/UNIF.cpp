#include "Core/Cartridge/UNIF.h"
#include <algorithm>
#include <array>
#include <cctype>

namespace R2NES::Core
{
    bool decodeSachenUNIF(const std::vector<uint8_t> &input, std::vector<uint8_t> &ines, std::string &error)
    {
        const auto fail = [&](const char *message)
        { error = message; return false; };
        const auto le32 = [&](size_t p)
        {
            return uint32_t(input[p]) | (uint32_t(input[p + 1]) << 8) |
                   (uint32_t(input[p + 2]) << 16) | (uint32_t(input[p + 3]) << 24);
        };
        if (input.size() < 32 || std::string(input.begin(), input.begin() + 4) != "UNIF")
            return fail("Truncated or invalid UNIF header");
        if (le32(4) > 7)
            return fail("Unsupported UNIF version");
        struct Chunk
        {
            size_t offset = 0, length = 0;
            bool present = false;
        };
        std::array<Chunk, 16> prg{}, chr{};
        std::string board;
        int mirror = -1;
        for (size_t p = 32; p < input.size();)
        {
            if (input.size() - p < 8)
                return fail("Truncated UNIF chunk header");
            const std::string tag(input.begin() + p, input.begin() + p + 4);
            const size_t length = le32(p + 4);
            p += 8;
            if (length > input.size() - p)
                return fail("Truncated UNIF chunk data");
            if (tag == "MAPR")
            {
                if (!board.empty() || length == 0 || length > 128 || input[p + length - 1] != 0)
                    return fail("Invalid or duplicate UNIF MAPR");
                board.assign(reinterpret_cast<const char *>(input.data() + p), length - 1);
                if (board.empty() || board.find('\0') != std::string::npos)
                    return fail("Invalid UNIF board name");
                const auto first = board.find_first_not_of(" \t\r\n"), last = board.find_last_not_of(" \t\r\n");
                if (first == std::string::npos)
                    return fail("Empty UNIF board name");
                board = board.substr(first, last - first + 1);
                std::transform(board.begin(), board.end(), board.begin(), [](unsigned char c)
                               { return std::toupper(c); });
            }
            else if (tag == "MIRR")
            {
                if (mirror != -1 || length != 1 || input[p] > 5)
                    return fail("Unsupported or duplicate UNIF mirroring");
                mirror = input[p];
            }
            else if (tag == "VROR" || ((tag == "BATR" || tag == "BATT") && (length != 1 || input[p] != 0)))
                return fail("Unsupported UNIF RAM/battery configuration");
            else if (tag.substr(0, 3) == "PRG" || tag.substr(0, 3) == "CHR")
            {
                const auto digit = std::string("0123456789ABCDEF").find(tag[3]);
                if (digit == std::string::npos || length == 0)
                    return fail("Invalid UNIF ROM chunk");
                auto &chunk = (tag[0] == 'P' ? prg : chr)[digit];
                if (chunk.present)
                    return fail("Duplicate UNIF ROM chunk");
                chunk = {p, length, true};
            }
            p += length;
        }
        if (board.compare(0, 4, "UNL-") == 0 || board.compare(0, 4, "NES-") == 0 || board.compare(0, 4, "HVC-") == 0)
            board.erase(0, 4);
        const unsigned mapper = board == "SA-016-1M" ? 79 : board == "TC-U01-1.5M"   ? 147
                                                        : board == "SACHEN-74LS374N" ? 150
                                                        : board == "CNROM"           ? 3
                                                                                     : 0;
        if (!mapper)
        {
            error = "Unsupported UNIF board: " + board;
            return false;
        }
        if (mirror < 0)
            return fail("Missing UNIF MIRR");
        // MIRR=5 é uma indicação de controle pelo mapper, não um modo H/V.
        // Os outros valores só são aceitos se a placa realmente os implementar.
        if (mapper == 150 ? mirror != 5 : mirror > 1)
            return fail("UNIF mirroring does not match board capabilities");
        const auto total = [](const std::array<Chunk, 16> &chunks)
        {
            size_t size = 0;
            bool gap = false;
            for (const auto &chunk : chunks)
            {
                if (!chunk.present)
                    gap = true;
                else
                {
                    if (gap || chunk.length > 131072 - size)
                        return size_t(0);
                    size += chunk.length;
                }
            }
            return size;
        };
        const size_t psize = total(prg), csize = total(chr);
        const size_t minPrg = mapper == 3 ? 16384 : 32768;
        const size_t maxPrg = mapper == 3 ? 32768 : mapper == 150 ? 131072
                                                                  : 65536;
        const size_t maxChr = mapper == 79 || mapper == 150 ? 65536 : 131072;
        if (psize < minPrg || psize > maxPrg || (psize & (psize - 1)) ||
            csize < 8192 || csize > maxChr || (csize & (csize - 1)))
            return fail("Unsupported UNIF PRG/CHR capacity or missing ROM chunks");
        std::vector<uint8_t> result(16, 0);
        result[0] = 'N';
        result[1] = 'E';
        result[2] = 'S';
        result[3] = 0x1A;
        result[4] = psize / 16384;
        result[5] = csize / 8192;
        result[6] = uint8_t((mapper & 15) << 4) | uint8_t(mirror == 1);
        result[7] = uint8_t(mapper & 0xF0) | 8;
        if (mapper == 3)
            result[8] = 0x20; // CNROM físico: submapper 2, conflitos AND.
        for (const auto &chunks : {prg, chr})
            for (const auto &chunk : chunks)
                if (chunk.present)
                    result.insert(result.end(), input.begin() + chunk.offset, input.begin() + chunk.offset + chunk.length);
        ines = std::move(result);
        return true;
    }
}
