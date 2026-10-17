#include "Core/Cartridge/UNIF.h"
#include "Core/Cartridge/Mappers/Mapper079.h"
#include "Core/Cartridge/Mappers/Mapper147.h"
#include "Core/Cartridge/Mappers/Mapper150.h"
#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;
using Bytes = std::vector<uint8_t>;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static void chunk(Bytes &b, const std::string &tag, const Bytes &data)
{
    b.insert(b.end(), tag.begin(), tag.end());
    for (unsigned s = 0; s < 32; s += 8)
        b.push_back(uint8_t(data.size() >> s));
    b.insert(b.end(), data.begin(), data.end());
}
static Bytes unif(bool tc)
{
    Bytes b(32, 0);
    b[0] = 'U';
    b[1] = 'N';
    b[2] = 'I';
    b[3] = 'F';
    b[4] = 7;
    const std::string board = tc ? "UNL-TC-U01-1.5M" : "UNL-SA-016-1M";
    Bytes name(board.begin(), board.end());
    name.push_back(0);
    chunk(b, "MAPR", name);
    // Chunks deliberadamente fora de ordem; os chips devem seguir o índice hexadecimal.
    chunk(b, "CHR1", Bytes(tc ? 65536 : 32768, 0x55));
    chunk(b, "PRG1", Bytes(32768, 0x22));
    chunk(b, "MIRR", {1});
    chunk(b, "NAME", {'T', 'e', 's', 't', 0});
    chunk(b, "PRG0", Bytes(32768, 0x11));
    chunk(b, "CHR0", Bytes(tc ? 65536 : 32768, 0x44));
    return b;
}
static std::filesystem::path save(const std::filesystem::path &dir, const std::string &name, const Bytes &bytes)
{
    const auto p = dir / name;
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(f.good(), "Fixture write failed");
    return p;
}
static std::string state(Mapper &m)
{
    std::stringstream s;
    m.saveState(s);
    return s.str();
}
static void generated(const std::filesystem::path &dir)
{
    for (bool tc : {false, true})
    {
        Bytes normalized;
        std::string error;
        const auto bytes = unif(tc);
        check(decodeSachenUNIF(bytes, normalized, error), "Valid UNIF rejected");
        check(normalized.size() == 16 + 65536 + (tc ? 131072 : 65536), "Wrong normalized size");
        check(normalized[16] == 0x11 && normalized[16 + 32768] == 0x22, "PRG chip order wrong");
        for (const std::string ext : {".nes", ".unf", ".unif"})
        {
            Cartridge c(save(dir, "fixture" + ext, bytes).string());
            check(c.isValid(), "UNIF extension/content detection failed");
            uint8_t d = 0;
            check(c.cpuRead(0x8000, d) && d == 0x11, "Initial PRG bank wrong");
            check(c.cpuWrite(tc ? 0x4102 : 0x4100, tc ? 0x44 : 0x0C, 0), "Sachen register rejected");
            check(c.cpuRead(0x8000, d) && d == 0x22, "Sachen PRG switch failed");
            check(c.ppuRead(0, d, 0) && d == 0x55, "Sachen CHR switch failed");
            check(!c.ppuWrite(0, 0, 0), "Sachen CHR ROM writable");
            Cartridge canonical(save(dir, "canonical.nes", normalized).string());
            check(c.getRomHash() == canonical.getRomHash(), "UNIF hash depends on wrapper metadata");
        }
        std::unique_ptr<Mapper> m = tc ? std::unique_ptr<Mapper>(new Mapper147(4, 16, MirrorMode::VERTICAL)) : std::unique_ptr<Mapper>(new Mapper079(4, 8, MirrorMode::VERTICAL));
        uint32_t mapped = 0;
        uint8_t d = 0;
        for (unsigned addr = 0; addr < 65536; ++addr)
        {
            const bool decoded = tc ? (addr & 0x4103) == 0x4102 : (addr & 0xE100) == 0x4100;
            check(m->cpuMapWrite(addr, mapped, 0xFF, 0) == decoded, "Register address decode mismatch");
        }
        for (unsigned value = 0; value < 256; ++value)
        {
            check(m->cpuMapWrite(tc ? 0x4102 : 0x4100, mapped, value, 0) && mapped == 0xFFFFFFFF, "Register sentinel missing");
            const unsigned p = tc ? (value >> 2) & 1 : (value >> 3) & 1, ch = tc ? (value >> 3) & 15 : value & 7;
            check(m->cpuMapRead(0xFFFF, mapped, d) && mapped == p * 32768 + 32767, "PRG latch bits wrong");
            check(m->ppuMapRead(0x1FFF, mapped, d, 0) && mapped == ch * 8192 + 8191, "CHR latch bits wrong");
        }
        const auto before = state(*m);
        for (size_t length : {0, 1})
        {
            std::stringstream s(before.substr(0, length));
            m->loadState(s);
            check(s.fail() && state(*m) == before, "Truncated state changed mapper");
        }
        std::stringstream bad(std::string{char(0xFF), char(0xFF)});
        m->loadState(bad);
        check(bad.fail() && state(*m) == before, "Bad state changed mapper");
        m->reset();
        std::stringstream restore(before);
        m->loadState(restore);
        check(restore.good() && state(*m) == before, "State restore failed");
        // Truncagens dos campos, payloads e comprimentos inválidos.
        for (size_t length : {size_t(0), size_t(4), size_t(31), size_t(33), bytes.size() - 1})
        {
            Bytes out{9};
            auto truncated = bytes;
            truncated.resize(length);
            check(!decodeSachenUNIF(truncated, out, error) && out == Bytes{9}, "Truncated UNIF accepted/committed");
        }
        auto badBytes = bytes;
        badBytes[36] = badBytes[37] = badBytes[38] = badBytes[39] = 255;
        check(!decodeSachenUNIF(badBytes, normalized, error), "Overflowing chunk accepted");
        badBytes = bytes;
        chunk(badBytes, "MIRR", {0});
        check(!decodeSachenUNIF(badBytes, normalized, error), "Duplicate MIRR accepted");
        badBytes = bytes;
        chunk(badBytes, "PRG0", {0});
        check(!decodeSachenUNIF(badBytes, normalized, error), "Duplicate PRG accepted");
        badBytes = bytes;
        chunk(badBytes, "BATR", {1});
        check(!decodeSachenUNIF(badBytes, normalized, error), "Unsupported battery accepted");
        badBytes = bytes;
        chunk(badBytes, "VROR", {});
        check(!decodeSachenUNIF(badBytes, normalized, error), "Unsupported CHR RAM accepted");
    }
    // Conflito AND somente no espelho de ROM do latch TC-U01.
    Bytes normalized;
    std::string error;
    check(decodeSachenUNIF(unif(true), normalized, error), "Fixture normalize failed");
    normalized[16 + 0x4102] = 0xFF;
    Cartridge c(save(dir, "conflicts.nes", normalized).string());
    c.cpuWrite(0xC102, 0x44, 0);
    check(state(*c.getMapper()) == std::string{1, 8}, "Conflict-safe high mirror failed");
    c.cpuWrite(0xC102, 0x44, 0);
    check(state(*c.getMapper()) == std::string{0, 0}, "High mirror did not AND current ROM");
    c.cpuWrite(0x4102, 0x44, 0);
    check(state(*c.getMapper()) == std::string{1, 8}, "Low register incorrectly applies conflicts");
    auto unknown = unif(false);
    unknown[40] = 'X';
    check(!decodeSachenUNIF(unknown, normalized, error), "Unknown board accepted");
}
static Bytes boardImage(const std::string &board, uint8_t mirror, unsigned prgSize = 65536, unsigned chrSize = 65536)
{
    Bytes b(32, 0);
    b[0] = 'U';
    b[1] = 'N';
    b[2] = 'I';
    b[3] = 'F';
    b[4] = 7;
    Bytes name(board.begin(), board.end());
    name.push_back(0);
    chunk(b, "MAPR", name);
    chunk(b, "MIRR", {mirror});
    Bytes prg(prgSize, 0xFF), chr(chrSize);
    for (unsigned bank = 0; bank < prgSize / 32768; ++bank)
        std::fill(prg.begin() + bank * 32768, prg.begin() + (bank + 1) * 32768, uint8_t(bank));
    prg[1] = 0xFF; // CNROM: endereço seguro para seleção de banco.
    for (unsigned bank = 0; bank < chrSize / 8192; ++bank)
        std::fill(chr.begin() + bank * 8192, chr.begin() + (bank + 1) * 8192, uint8_t(0x40 + bank));
    chunk(b, "PRG0", prg);
    chunk(b, "CHR0", chr);
    return b;
}
static void mapper150(const std::filesystem::path &dir)
{
    Mapper150 m(8, 8, MirrorMode::HORIZONTAL);
    uint32_t mapped = 0;
    uint8_t data = 0;
    const auto write = [&](unsigned reg, unsigned value)
    {
        check(m.cpuMapWrite(0x4100, mapped, reg, 0) && m.cpuMapWrite(0x4101, mapped, value, 0), "150 register write rejected");
        check(mapped == 0xFFFFFFFF, "150 register sentinel missing");
    };
    for (unsigned reg = 0; reg < 8; ++reg)
        for (unsigned value = 0; value < 256; ++value)
        {
            write(reg, value);
            data = 0xA8;
            check(m.cpuMapRead(0x4101, mapped, data) && data == (0xA8 | (value & 7)), "150 readable register/open bus mismatch");
        }
    write(4, 1);
    write(6, 3);
    check(m.ppuMapRead(0x1FFF, mapped, data, 0) && mapped == 65535, "150 split CHR bank bits wrong");
    for (unsigned bank = 0; bank < 4; ++bank)
    {
        write(5, bank);
        for (uint16_t addr : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
            check(m.cpuMapRead(addr, mapped, data) && mapped == bank * 32768u + (addr & 32767), "150 PRG window mismatch");
    }
    const auto before = state(m);
    for (unsigned addr = 0; addr < 65536; ++addr)
    {
        const auto port = addr & 0xC101;
        check(m.cpuMapWrite(addr, mapped, 7, 0) == (port == 0x4100 || port == 0x4101), "150 address decode mismatch");
    }
    std::stringstream restore(before);
    m.loadState(restore);
    check(restore.good() && state(m) == before, "150 state restore failed");
    for (size_t length = 0; length < before.size(); ++length)
    {
        std::stringstream bad(before.substr(0, length));
        m.loadState(bad);
        check(bad.fail() && state(m) == before, "150 truncated state committed");
    }
    for (size_t field = 0; field < before.size(); ++field)
    {
        auto bytes = before;
        bytes[field] = 8;
        std::stringstream bad(bytes);
        m.loadState(bad);
        check(bad.fail() && state(m) == before, "150 invalid state committed");
    }
    check(!m.ppuMapWrite(0, mapped, 0, 0) && !m.ppuMapRead(0x2000, mapped, data, 0), "150 CHR protection/range failed");
    for (unsigned prg : {2, 4, 8})
        for (unsigned chr : {1, 2, 4, 8})
        {
            Mapper150 small(prg, chr, MirrorMode::VERTICAL);
            uint32_t a = 0;
            small.cpuMapWrite(0x4100, a, 5, 0);
            small.cpuMapWrite(0x4101, a, 7, 0);
            check(small.cpuMapRead(0xFFFF, a, data) && a == prg * 16384u - 1, "150 physical PRG mirroring wrong");
            small.cpuMapWrite(0x4100, a, 4, 0);
            small.cpuMapWrite(0x4101, a, 1, 0);
            small.cpuMapWrite(0x4100, a, 6, 0);
            small.cpuMapWrite(0x4101, a, 7, 0);
            check(small.ppuMapRead(0x1FFF, a, data, 0) && a == chr * 8192u - 1, "150 physical CHR mirroring wrong");
        }
    Bytes normalized;
    std::string error;
    const auto bytes = boardImage("UNL-Sachen-74LS374N", 5, 131072);
    check(decodeSachenUNIF(bytes, normalized, error) && normalized[6] == 0x60 && normalized[7] == 0x98, "MIRR 5 incorrectly encoded as iNES flags");
    NES nes;
    nes.insertCartridge(save(dir, "150.nes", bytes).string());
    check(nes.isCartridgeLoaded(), "150 UNIF load failed");
    auto &bus = nes.getBus();
    auto &ppu = nes.getPpu();
    const unsigned pages[4][4] = {{0, 0, 0, 1}, {0, 0, 1, 1}, {0, 1, 0, 1}, {1, 1, 1, 1}};
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        // Inicializa as duas páginas físicas pela configuração vertical.
        bus.cpuWrite(0x4100, 7);
        bus.cpuWrite(0x4101, 4);
        for (unsigned offset = 0; offset < 1024; ++offset)
        {
            ppu.ppuWrite(0x2000 + offset, uint8_t(offset));
            ppu.ppuWrite(0x2400 + offset, uint8_t(offset + 73));
        }
        bus.cpuWrite(0x4101, mode << 1);
        for (unsigned table = 0; table < 4; ++table)
            for (unsigned offset = 0; offset < 1024; ++offset)
                check(ppu.ppuRead(0x2000 + table * 1024 + offset) == uint8_t(offset + pages[mode][table] * 73), "150 CIRAM page arrangement wrong");
    }
    bus.cpuWrite(0x4100, 3);
    bus.cpuWrite(0x4101, 5);
    check(bus.cpuRead(0x4101) == 5, "150 bus readback wrong");
    bus.cpuWrite(0x4100, 5);
    bus.cpuWrite(0x4101, 3);
    check(nes.cpuRead(0xFFFF) == 3, "150 console bank switch failed");
    std::stringstream saved;
    check(nes.saveState(saved), "150 console save failed");
    const auto snap = saved.str();
    auto corrupt = snap;
    corrupt[corrupt.size() - 9] = 8;
    std::stringstream bad(corrupt);
    check(!nes.loadState(bad), "150 console accepted bad index");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == snap, "150 console failed rollback");
    nes.reset();
    std::stringstream input(snap);
    check(nes.loadState(input), "150 console restore failed");
    for (unsigned mirr : {0, 1, 2, 3, 4})
        check(!decodeSachenUNIF(boardImage("UNL-Sachen-74LS374N", mirr), normalized, error), "150 incompatible MIRR accepted");
    for (unsigned mirr : {2, 3, 4, 5})
        check(!decodeSachenUNIF(boardImage("NES-CNROM", mirr, 32768, 32768), normalized, error), "CNROM incompatible MIRR accepted");
    auto duplicate = bytes;
    chunk(duplicate, "MIRR", {5});
    check(!decodeSachenUNIF(duplicate, normalized, error), "Duplicate MIRR 5 accepted");
    const auto cnrom = boardImage("NES-CNROM", 0, 32768, 32768);
    check(decodeSachenUNIF(cnrom, normalized, error) && normalized[8] == 0x20, "CNROM submapper conflicts not declared");
    Cartridge c(save(dir, "cnrom.unf", cnrom).string());
    check(c.isValid(), "UNIF CNROM rejected");
    c.cpuWrite(0x8001, 3, 0);
    check(c.ppuRead(0, data, 0) && data == 0x43, "UNIF CNROM CHR select wrong");
    c.cpuWrite(0x8000, 3, 0);
    check(c.ppuRead(0, data, 0) && data == 0x40, "UNIF CNROM conflict missing");
}
static void boot(const char *path)
{
    NES nes;
    nes.insertCartridge(path);
    check(nes.isCartridgeLoaded(), "Real Sachen archive rejected");
    unsigned frames = 0;
    uint32_t first = 0;
    bool visible = false;
    for (unsigned step = 0; step < 6000000; ++step)
    {
        nes.step();
        if (nes.isFrameComplete())
        {
            ++frames;
            nes.clearFrameComplete();
            const auto *pixels = nes.getPpu().getFrameBuffer();
            first = pixels[0];
            for (unsigned i = 1; i < 256 * 240; ++i)
                if (pixels[i] != first)
                {
                    visible = true;
                    break;
                }
        }
    }
    check(frames > 100 && visible, "No visible video during Sachen boot");
    std::stringstream s;
    check(nes.saveState(s), "Real ROM save failed");
    const auto bytes = s.str();
    for (unsigned i = 0; i < 100; ++i)
        nes.step();
    std::stringstream restore(bytes);
    check(nes.loadState(restore), "Real ROM state load failed");
    std::stringstream after;
    check(nes.saveState(after) && after.str() == bytes, "Real ROM state round trip failed");
    std::cout << "Boot: " << path << "; frames=" << frames << "; hash=" << nes.getBus().cart->getRomHash() << "; visible video and state restore OK\n";
}
int main(int argc, char **argv)
{
    try
    {
        check(argc >= 2, "Pass fixture directory");
        std::filesystem::create_directories(argv[1]);
        generated(argv[1]);
        mapper150(argv[1]);
        for (int i = 2; i < argc; ++i)
            boot(argv[i]);
        std::cout << "Sachen/UNIF tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
