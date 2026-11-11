#include "System/RomReader.h"
using R2NES::System::readRomFile;
#include "Core/Cartridge/Cartridge.h"
#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <zlib.h>
#include "TestFixtures.h"

using namespace R2NES::Core;
using Bytes = std::vector<uint8_t>;
static void check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
static void put16(Bytes &v, uint16_t n)
{
    v.push_back(uint8_t(n));
    v.push_back(uint8_t(n >> 8));
}
static void put32(Bytes &v, uint32_t n)
{
    put16(v, uint16_t(n));
    put16(v, uint16_t(n >> 16));
}
static void set32(Bytes &v, size_t p, uint32_t n)
{
    for (int i = 0; i < 4; ++i)
        v[p + i] = uint8_t(n >> (8 * i));
}

static Bytes image()
{
    Bytes v(16 + 16384, 0);
    v[0] = 'N';
    v[1] = 'E';
    v[2] = 'S';
    v[3] = 0x1a;
    v[4] = 1;
    return v;
}

static Bytes archive(const Bytes &raw, bool deflated, bool descriptor, const std::string &name = "folder/game.nes")
{
    Bytes payload = raw;
    if (deflated)
    {
        z_stream s{};
        check(deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK, "deflate init");
        payload.resize(compressBound(raw.size()));
        s.next_in = const_cast<Bytef *>(raw.data());
        s.avail_in = static_cast<uInt>(raw.size());
        s.next_out = payload.data();
        s.avail_out = static_cast<uInt>(payload.size());
        check(deflate(&s, Z_FINISH) == Z_STREAM_END, "deflate");
        payload.resize(s.total_out);
        deflateEnd(&s);
    }
    const uint32_t crc = crc32(0, raw.data(), static_cast<uInt>(raw.size()));
    const uint16_t flags = descriptor ? 8 : 0, method = deflated ? 8 : 0;
    Bytes v;
    put32(v, 0x04034b50);
    put16(v, 20);
    put16(v, flags);
    put16(v, method);
    put32(v, 0);
    put32(v, descriptor ? 0 : crc);
    put32(v, descriptor ? 0 : uint32_t(payload.size()));
    put32(v, descriptor ? 0 : uint32_t(raw.size()));
    put16(v, uint16_t(name.size()));
    put16(v, 0);
    v.insert(v.end(), name.begin(), name.end());
    v.insert(v.end(), payload.begin(), payload.end());
    if (descriptor)
    {
        put32(v, 0x08074b50);
        put32(v, crc);
        put32(v, uint32_t(payload.size()));
        put32(v, uint32_t(raw.size()));
    }
    const uint32_t central = uint32_t(v.size());
    put32(v, 0x02014b50);
    put16(v, 20);
    put16(v, 20);
    put16(v, flags);
    put16(v, method);
    put32(v, 0);
    put32(v, crc);
    put32(v, uint32_t(payload.size()));
    put32(v, uint32_t(raw.size()));
    put16(v, uint16_t(name.size()));
    put16(v, 0);
    put16(v, 0);
    put16(v, 0);
    put16(v, 0);
    put32(v, 0);
    put32(v, 0);
    v.insert(v.end(), name.begin(), name.end());
    const uint32_t centralSize = uint32_t(v.size()) - central;
    put32(v, 0x06054b50);
    put16(v, 0);
    put16(v, 0);
    put16(v, 1);
    put16(v, 1);
    put32(v, centralSize);
    put32(v, central);
    put16(v, 0);
    return v;
}

static void save(const std::filesystem::path &path, const Bytes &v)
{
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(v.data()), v.size());
    check(bool(f), "write fixture");
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2 || argc == 3, "fixture directory required");
        const std::filesystem::path dir = argv[1];
        std::filesystem::create_directories(dir);
        const Bytes raw = image();
        for (bool deflated : {false, true})
            for (bool descriptor : {false, true})
            {
                const auto zip = archive(raw, deflated, descriptor, "folder/GAME.NES");
                Bytes out{42};
                check(extractRomFromZip(zip, out) && out == raw, "valid ZIP rejected");
                save(dir / "valid.zip", zip);
                FixtureCartridge c((dir / "valid.zip").string());
                check(c.isValid(), "valid ZIP cartridge rejected");
            }
        const Bytes valid = archive(raw, true, false);
        const size_t eocd = valid.size() - 22;
        const size_t cd = uint32_t(valid[eocd + 16]) | (uint32_t(valid[eocd + 17]) << 8) |
                          (uint32_t(valid[eocd + 18]) << 16) | (uint32_t(valid[eocd + 19]) << 24);
        const auto reject = [](const Bytes &v)
        { Bytes out{42}; check(!extractRomFromZip(v, out) && out == Bytes{42}, "invalid ZIP accepted or output changed"); };
        for (size_t n = 0; n < valid.size(); ++n)
            reject(Bytes(valid.begin(), valid.begin() + n));
        auto bad = valid;
        set32(bad, cd + 24, 0xffffffff);
        reject(bad); // ZIP bomb / ZIP64 size
        bad = valid;
        set32(bad, cd + 20, 0xffffffff);
        reject(bad);
        bad = valid;
        set32(bad, cd + 42, 0xffffffff);
        reject(bad);
        bad = valid;
        set32(bad, eocd + 16, 0xffffffff);
        reject(bad);
        bad = valid;
        bad[cd + 8] |= 1;
        reject(bad); // encryption
        bad = valid;
        bad[cd + 10] = 99;
        reject(bad); // method
        bad = valid;
        bad[cd + 16] ^= 1;
        reject(bad); // CRC mismatch
        bad = valid;
        bad[eocd + 4] = 1;
        reject(bad); // multi-disk
        bad = valid;
        bad[eocd + 10] = 2;
        reject(bad); // mismatched entry counts
        bad = valid;
        set32(bad, eocd + 12, 0xffffffff);
        reject(bad);
        bad = valid;
        bad[30] ^= 1;
        reject(bad); // local/central name disagreement
        bad = valid;
        bad[cd + 28] = 0xff;
        bad[cd + 29] = 0xff;
        reject(bad);
        bad = valid;
        bad[6] |= 1;
        reject(bad); // local/central flags disagree
        bad = archive(raw, false, false);
        bad[30 + std::string("folder/game.nes").size()] ^= 1;
        reject(bad);
        bad = valid;
        bad[30 + std::string("folder/game.nes").size()] ^= 0xff;
        reject(bad);
        reject(archive(raw, false, false, "readme.txt"));

        // Deterministic bounded mutation corpus for sanitizer runs; acceptance
        // is allowed only within the documented output limit.
        uint32_t seed = 0x184ad937;
        const auto random = [&seed]()
        { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; };
        for (int iteration = 0; iteration < 2000; ++iteration)
        {
            auto mutated = valid;
            for (unsigned j = 0, count = 1 + random() % 4; j < count; ++j)
                mutated[random() % mutated.size()] = static_cast<uint8_t>(random());
            Bytes out;
            if (extractRomFromZip(mutated, out))
                check(!out.empty() && out.size() <= MaxRomFileSize, "mutation exceeded output limit");
        }

        const auto path = dir / "direct.nes";
        save(path, raw);
        FixtureNES nes("");
        nes.setCheatDirectory(dir / "no-cheats");
        nes.insertCartridge(path.string());
        check(nes.getBus().cart && nes.getBus().cart->isValid(), "initial cartridge");
        auto previous = nes.getBus().cart;
        std::ostringstream before;
        check(nes.saveState(before), "save before failed insertion");
        save(dir / "bad.zip", bad);
        check(!nes.insertCartridge((dir / "bad.zip").string()), "bad ZIP accepted");
        check(nes.isCartridgeLoaded() && nes.getBus().cart == previous, "bad ZIP replaced cartridge or unloaded session");
        std::ostringstream after;
        check(nes.saveState(after) && before.str() == after.str(), "bad ZIP changed console state");
        std::ofstream large(dir / "large.nes", std::ios::binary);
        large.close();
        std::filesystem::resize_file(dir / "large.nes", MaxRomFileSize + 1);
        FixtureCartridge oversized((dir / "large.nes").string());
        check(!oversized.isValid(), "oversized ROM accepted");
        std::filesystem::remove(dir / "large.nes");
        if (argc == 3)
        {
            FixtureCartridge external(argv[2]);
            check(external.isValid(), "independently generated ZIP rejected");
        }
        std::cout << "ROM and ZIP bounds tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}