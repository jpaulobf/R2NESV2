#include "Core/Cartridge/Mappers/Mapper011.h"
#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static std::string state(Mapper &m)
{
    std::stringstream s;
    m.saveState(s);
    return s.str();
}
static unsigned prgCount(unsigned units) { return std::max(1u, units / 2); }
static unsigned chrCount(unsigned units) { return std::max(1u, units); }

static void mapperTests()
{
    for (uint16_t prg : {1, 2, 4, 8})
        for (uint16_t chr : {0, 1, 2, 4, 8, 16})
            for (MirrorMode mirror : {MirrorMode::HORIZONTAL, MirrorMode::VERTICAL})
            {
                Mapper011 m(prg, chr, mirror);
                uint32_t mapped = 0;
                uint8_t data = 0;
                for (uint16_t reg : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                    for (unsigned value = 0; value < 256; ++value)
                    {
                        check(m.cpuMapWrite(reg, mapped, value, 0) && mapped == 0xFFFFFFFF, "Register write not handled internally");
                        const unsigned p = value & (prgCount(prg) - 1), c = (value >> 4) & (chrCount(chr) - 1);
                        for (uint16_t addr : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                            check(m.cpuMapRead(addr, mapped, data) && mapped == (p * 32768 + (addr & 32767)) % (prg * 16384), "PRG bank/window mismatch");
                        for (uint16_t addr : {0, 0x0FFF, 0x1000, 0x1FFF})
                        {
                            check(m.ppuMapRead(addr, mapped, data, 0) && mapped == c * 8192 + addr, "CHR bank/window mismatch");
                            check(m.ppuMapWrite(addr, mapped, 0xA5, 0) == (chr == 0), "Incorrect CHR RAM/ROM protection");
                            if (!chr)
                                check(mapped == addr, "CHR RAM not fixed");
                        }
                        check(m.getMirrorMode() == mirror, "Register or lockout bits changed fixed mirroring");
                    }
                const auto before = state(m);
                for (uint16_t addr : {0x0000, 0x4020, 0x6000, 0x7FFF})
                {
                    check(!m.cpuMapRead(addr, mapped, data), "Unexpected PRG RAM");
                    check(!m.cpuMapWrite(addr, mapped, 0, 0) && state(m) == before, "Unexpected low register");
                }
                check(!m.ppuMapRead(0x2000, mapped, data, 0) && !m.ppuMapWrite(0x2000, mapped, 0, 0), "CHR outside pattern space");
                m.reset();
                check(state(m) == std::string(2, 0) && m.getMirrorMode() == mirror, "Reset changed hardware mirroring");
                check(!m.getIrqFlag(), "Mapper 11 generated IRQ");
            }
    for (const auto config : {std::pair<unsigned, unsigned>{0, 1}, {3, 1}, {16, 1}, {256, 1}, {8, 3}, {8, 32}})
    {
        bool rejected = false;
        try
        {
            Mapper011 m(config.first, config.second, MirrorMode::HORIZONTAL);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        check(rejected, "Invalid direct configuration accepted");
    }
    for (MirrorMode mirror : {MirrorMode::FOUR_SCREEN, MirrorMode::ONESCREEN_LO, MirrorMode::ONESCREEN_HI})
    {
        bool rejected = false;
        try
        {
            Mapper011 m(8, 16, mirror);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        check(rejected, "Unsupported direct mirroring accepted");
    }
}
static void states()
{
    for (uint16_t prg : {1, 2, 4, 8})
        for (uint16_t chr : {0, 1, 2, 4, 8, 16})
        {
            Mapper011 m(prg, chr, MirrorMode::VERTICAL);
            for (unsigned p = 0; p < prgCount(prg); ++p)
                for (unsigned c = 0; c < chrCount(chr); ++c)
                {
                    const std::string bytes{char(p), char(c)};
                    std::stringstream input(bytes);
                    m.loadState(input);
                    check(input.good() && state(m) == bytes, "Legacy two-byte state rejected");
                }
            const auto before = state(m);
            for (size_t length : {0, 1})
            {
                std::stringstream input(std::string(length, 0));
                m.loadState(input);
                check(input.fail() && state(m) == before, "Truncated state changed mapper");
            }
            for (unsigned field : {0, 1})
            {
                auto bytes = before;
                bytes[field] = char(field ? chrCount(chr) : prgCount(prg));
                std::stringstream input(bytes);
                m.loadState(input);
                check(input.fail() && state(m) == before, "Out-of-range state changed mapper");
            }
        }
}
static std::vector<uint8_t> rom(unsigned prg, unsigned chr, bool nes2, bool vertical = false)
{
    std::vector<uint8_t> r(16 + prg * 16384 + chr * 8192, 0);
    r[0] = 'N';
    r[1] = 'E';
    r[2] = 'S';
    r[3] = 0x1A;
    r[4] = prg;
    r[5] = chr;
    r[6] = 0xB0 | (vertical ? 1 : 0);
    r[7] = nes2 ? 8 : 0;
    r[11] = nes2 && !chr ? 7 : 0;
    for (unsigned b = 0; b < prg; ++b)
    {
        auto first = r.begin() + 16 + b * 16384;
        std::fill(first, first + 16384, uint8_t(0xF0 | b));
        first[1] = 0xFF; // Endereço sem conflito para selecionar um banco conhecido.
        first[2] = (b / 2 == 3) ? 0x21 : (b / 2 == 1) ? 0x32
                                                      : 0x10;
    }
    for (unsigned b = 0; b < chr; ++b)
        std::fill(r.begin() + 16 + prg * 16384 + b * 8192, r.begin() + 16 + prg * 16384 + (b + 1) * 8192, uint8_t(0x40 + b));
    return r;
}
static std::filesystem::path saveRom(const std::filesystem::path &dir, const std::vector<uint8_t> &bytes)
{
    const auto path = dir / "mapper011-fixture.nes";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(output.good(), "Fixture write failed");
    return path;
}
static unsigned read(Cartridge &cart, uint16_t address)
{
    uint8_t data = 0;
    check(cart.cpuRead(address, data), "Cartridge CPU read rejected");
    return data;
}
static void images(const std::filesystem::path &dir)
{
    for (bool nes2 : {false, true})
        for (unsigned prg : {1, 2, 4, 8})
            for (unsigned chr : {0, 1, 2, 4, 8, 16})
                for (bool vertical : {false, true})
                {
                    const auto bytes = rom(prg, chr, nes2, vertical);
                    Cartridge cart(saveRom(dir, bytes).string());
                    check(cart.isValid(), "Valid Color Dreams cartridge rejected");
                    unsigned selected = 0;
                    for (uint16_t reg : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                        for (unsigned value = 0; value < 256; ++value)
                        {
                            const unsigned physical = (selected * 32768 + (reg & 32767)) % (prg * 16384);
                            const unsigned effective = value & bytes[16 + physical];
                            check(cart.cpuWrite(reg, value, 0), "Cartridge register write rejected");
                            selected = effective & (prgCount(prg) - 1);
                            check(state(*cart.getMapper()) == std::string{char(selected), char((effective >> 4) & (chrCount(chr) - 1))},
                                  "AND conflict/PRG/CHR latch mismatch");
                            for (uint16_t addr : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
                                check(read(cart, addr) == bytes[16 + (selected * 32768 + (addr & 32767)) % (prg * 16384)], "PRG ROM changed or wrong bank");
                            uint8_t data = 0;
                            check(cart.ppuRead(0x1FFF, data, 0) && data == (chr ? 0x40 + ((effective >> 4) & (chr - 1)) : 0), "Cartridge CHR bank mismatch");
                            check(cart.getMirrorMode() == (vertical ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL), "Fixed mirror changed");
                        }
                    uint8_t data = 0;
                    check(cart.ppuWrite(0x1FFF, 0xA5, 0) == (chr == 0), "CHR protection failed");
                    if (!chr)
                        check(cart.ppuRead(0x1FFF, data, 0) && data == 0xA5, "CHR RAM write lost");
                }
    // Os conflitos precisam ler o banco anterior à escrita, e não o destino.
    Cartridge c(saveRom(dir, rom(8, 16, true)).string());
    check(c.cpuWrite(0x8001, 0xF3, 0), "Safe bank select rejected");
    check(state(*c.getMapper()) == std::string{3, 15}, "Safe bank select failed");
    c.cpuWrite(0x8002, 0xF3, 0);
    check(state(*c.getMapper()) == std::string{1, 2}, "First conflict used wrong bank");
    c.cpuWrite(0x8002, 0xF3, 0);
    check(state(*c.getMapper()) == std::string{2, 3}, "Second conflict used stale bank");
    for (unsigned prg : {1, 2, 4, 8})
        for (unsigned chr : {0, 1, 2, 4, 8, 16})
        {
            auto bytes = rom(prg, chr, true);
            unsigned p = 14, ch = 13;
            for (unsigned n = prg; n > 1; n >>= 1)
                ++p;
            bytes[4] = p << 2;
            bytes[9] = 0x0F;
            if (chr)
            {
                for (unsigned n = chr; n > 1; n >>= 1)
                    ++ch;
                bytes[5] = ch << 2;
                bytes[9] |= 0xF0;
            }
            Cartridge cart(saveRom(dir, bytes).string());
            check(cart.isValid(), "Exponent/multiplier header rejected");
        }
}
static void invalid(const std::filesystem::path &dir)
{
    auto reject = [&](const std::vector<uint8_t> &bytes)
    {Cartridge c(saveRom(dir,bytes).string());check(!c.isValid()&&!c.getMapper(),"Invalid image accepted"); };
    for (bool nes2 : {false, true})
    {
        for (unsigned p : {0, 3, 16})
            reject(rom(p, 1, nes2));
        for (unsigned ch : {3, 32})
            reject(rom(8, ch, nes2));
        auto bytes = rom(8, 16, nes2);
        bytes.resize(16 + 65536);
        reject(bytes);
        bytes = rom(8, 16, nes2);
        bytes.pop_back();
        reject(bytes);
        for (uint8_t flag : {2, 4, 8})
        {
            bytes = rom(8, 16, nes2);
            bytes[6] |= flag;
            if (flag == 4)
                bytes.insert(bytes.begin() + 16, 512, 0);
            reject(bytes);
        }
    }
    for (unsigned sub = 1; sub < 16; ++sub)
    {
        auto bytes = rom(8, 16, true);
        bytes[8] = sub << 4;
        reject(bytes);
    }
    for (uint8_t ram : {1, 7, 0x70, 0x77})
    {
        auto bytes = rom(8, 16, true);
        bytes[10] = ram;
        reject(bytes);
    }
    for (uint8_t ram : {0, 6, 8, 0x70, 0x77})
    {
        auto bytes = rom(8, 0, true);
        bytes[11] = ram;
        reject(bytes);
    }
    auto bytes = rom(8, 16, true);
    bytes[11] = 7;
    reject(bytes);
    bytes = rom(8, 16, true);
    bytes[14] = 1;
    reject(bytes);
    bytes = rom(8, 16, true);
    bytes[8] = 1;
    reject(bytes); // Mapper 267 não pode virar 11.
    bytes = rom(8, 16, true);
    bytes[9] = 15;
    bytes[4] = (14 << 2) | 1;
    reject(bytes);
    bytes[4] = 255;
    reject(bytes);
    // O byte 8 de iNES antigo não é submapper nem cria PRG-RAM nesta placa.
    bytes = rom(8, 16, false);
    bytes[8] = 0x20;
    Cartridge legacy(saveRom(dir, bytes).string());
    check(legacy.isValid(), "Legacy byte 8 misread as submapper");
}
static std::string consoleState(NES &nes)
{
    std::stringstream s;
    check(nes.saveState(s), "Console save failed");
    return s.str();
}
static void console(const std::filesystem::path &dir)
{
    for (unsigned chr : {0, 16})
        for (bool vertical : {false, true})
        {
            NES nes;
            nes.insertCartridge(saveRom(dir, rom(8, chr, true, vertical)).string());
            check(nes.isCartridgeLoaded(), "Console insert failed");
            auto &bus = nes.getBus();
            auto &ppu = nes.getPpu();
            bus.cpuWrite(0x8001, 0xF3);
            check(nes.cpuRead(0xFFFF) == 0xF7, "Console PRG bank mismatch");
            for (unsigned addr = 0; addr < 8192; ++addr)
            {
                ppu.ppuWrite(addr, uint8_t(addr ^ (addr >> 8)));
                const uint8_t expected = chr ? 0x4F : uint8_t(addr ^ (addr >> 8));
                check(ppu.ppuRead(addr) == expected && ppu.ppuReadSprite(addr) == expected, "Console CHR ROM protection/RAM mismatch");
            }
            ppu.ppuWrite(0x2000, 0x31);
            ppu.ppuWrite(0x2C00, 0x52);
            check(ppu.ppuRead(vertical ? 0x2800 : 0x2400) == 0x31 && ppu.ppuRead(vertical ? 0x2400 : 0x2800) == 0x52, "PPU H/V mirror mismatch");
            bus.cpuWrite(0x6000, 0xA5);
            check(bus.cpuRead(0x6000) == 0xA5, "Absent PRG RAM not open bus");
            bus.cpuRead(0xFFFF);
            check(bus.cpuRead(0x6000) == 0xF7, "Phantom PRG RAM retained write");
            const auto before = consoleState(nes);
            const size_t start = before.size() - (chr ? 0 : 8192) - 2;
            for (size_t length : {start, start + 1, before.size() - 1})
            {
                std::stringstream broken(before.substr(0, length));
                check(!nes.loadState(broken) && consoleState(nes) == before, "Truncated console load failed rollback");
            }
            auto bad = before;
            bad[start + 1] = char(0xFF);
            std::stringstream broken(bad);
            check(!nes.loadState(broken) && consoleState(nes) == before, "Corrupt mapper state failed rollback");
            nes.reset();
            check(state(*bus.cart->getMapper()) == std::string(2, 0), "Console reset did not clear banks");
            if (!chr)
            {
                check(ppu.ppuRead(0x1FFF) == uint8_t(0x1FFF ^ 0x1F), "Reset erased cartridge RAM");
                ppu.ppuWrite(0x1FFF, 0);
            }
            std::stringstream restore(before);
            check(nes.loadState(restore) && consoleState(nes) == before, "Console restore failed");
        }
}
int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated test ROMs");
        std::filesystem::create_directories(argv[1]);
        mapperTests();
        states();
        images(argv[1]);
        invalid(argv[1]);
        console(argv[1]);
        std::cout << "Mapper 11 tests passed (banks, conflicts, mirroring, CHR ROM/RAM, states, console, invalid images)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
