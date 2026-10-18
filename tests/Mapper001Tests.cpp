#include "Core/Cartridge/Mappers/Mapper001.h"
#include "Core/NES.h"
#include "Core/Cartridge/BatterySave.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <chrono>

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static uint32_t mapped(Mapper001 &m, uint16_t address)
{
    uint32_t result = 0;
    uint8_t data = 0;
    check(m.cpuMapRead(address, result, data), "PRG read rejected");
    return result;
}
static void writeRegister(Mapper &m, uint16_t address, uint8_t value, uint32_t &clock)
{
    uint32_t result = 0;
    for (unsigned bit = 0; bit < 5; ++bit)
    {
        clock += 4;
        check(m.cpuMapWrite(address, result, (value >> bit) & 1, clock), "Register write rejected");
        check(result == 0xFFFFFFFF, "Register write mapped to ROM");
    }
}
static std::string state(Mapper &m)
{
    std::stringstream s;
    m.saveState(s);
    return s.str();
}
static std::vector<uint8_t> rom(unsigned prg, unsigned chr, unsigned ram = 7, unsigned sub = 0)
{
    std::vector<uint8_t> r(16 + prg * 0x4000 + chr * 0x2000, 0);
    r[0] = 'N';
    r[1] = 'E';
    r[2] = 'S';
    r[3] = 0x1A;
    r[4] = prg;
    r[5] = chr;
    r[6] = 0x10 | ((ram & 0xF0) ? 2 : 0);
    r[7] = 8;
    r[8] = sub << 4;
    r[10] = ram;
    r[11] = chr ? 0 : 7;
    for (unsigned b = 0; b < prg; ++b)
        std::fill(r.begin() + 16 + b * 0x4000, r.begin() + 16 + (b + 1) * 0x4000, b);
    return r;
}
static std::filesystem::path saveRom(const std::filesystem::path &dir, const std::vector<uint8_t> &bytes)
{
    auto path = dir / "mapper001-fixture.nes";
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(f.good(), "Fixture write failed");
    return path;
}
static unsigned read(Cartridge &c, uint16_t address)
{
    uint8_t value = 0;
    check(c.cpuRead(address, value), "Cartridge read rejected");
    return value;
}

static void battery(const std::filesystem::path &dir)
{
    using Config = Mapper001::Configuration;
    uint32_t mappedAddress = 0;
    Mapper001 volatileOnly(16, 0, Config{});
    check(volatileOnly.getBatteryData().empty(), "Volatile RAM exposed as battery data");
    const auto unchanged = state(volatileOnly);
    check(!volatileOnly.setBatteryData({1}) && state(volatileOnly) == unchanged, "Invalid battery import changed mapper");
    for (unsigned chrBanks : {0, 8})
    {
        Config config;
        config.prgRamSize = config.prgNvRamSize = 8192;
        Mapper001 mapper(16, chrBanks, config);
        uint32_t time = 0;
        const unsigned select = chrBanks ? 16 : 8; // SZROM / SOROM.
        writeRegister(mapper, 0xA000, 0, time);
        mapper.cpuMapWrite(0x6000, mappedAddress, 0x11, time + 2);
        writeRegister(mapper, 0xA000, select, time);
        mapper.cpuMapWrite(0x6000, mappedAddress, 0x22, time + 2);
        const auto bytes = mapper.getBatteryData();
        check(bytes.size() == 8192 && bytes.front() == 0x22, "Wrong SOROM/SZROM chip persisted");
        std::vector<uint8_t> restored(8192, 0x33);
        const auto before = state(mapper);
        check(!mapper.setBatteryData({1}) && state(mapper) == before, "Short battery import was not atomic");
        check(mapper.setBatteryData(restored), "Valid battery import rejected");
        writeRegister(mapper, 0xA000, 0, time);
        uint8_t value = 0;
        mapper.cpuMapRead(0x6000, mappedAddress, value);
        check(value == 0x11, "Battery import overwrote volatile chip");
        writeRegister(mapper, 0xA000, select, time);
        mapper.cpuMapRead(0x6000, mappedAddress, value);
        check(value == 0x33, "Battery import missed persistent chip");
        mapper.reset();
        check(mapper.getBatteryData() == restored, "Reset erased persistent memory");
    }
    Config sx;
    sx.prgRamSize = 0;
    sx.prgNvRamSize = 32768;
    Mapper001 banked(32, 0, sx);
    uint32_t time = 0;
    for (unsigned bank = 0; bank < 4; ++bank)
    {
        writeRegister(banked, 0xA000, bank << 2, time);
        banked.cpuMapWrite(0x6000, mappedAddress, 0x40 + bank, time + 2);
    }
    const auto banks = banked.getBatteryData();
    for (unsigned bank = 0; bank < 4; ++bank)
        check(banks[bank * 8192] == 0x40 + bank, "SXROM battery order does not follow physical address lines");

    const auto root = dir / ("battery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto fileBytes = [](const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
    };
    auto bytes = rom(16, 0, 0x70);
    // iNES convencional de Zelda: bit de bateria, byte 8 sem tamanho explícito.
    bytes[7] = 0;
    bytes[8] = 0;
    bytes[10] = 0;
    const auto path = saveRom(root, bytes);
    Cartridge identity(path.string());
    const auto saves = root / "saves", save = saves / (identity.getRomHash() + ".sav");
    {
        NES nes(saves);
        nes.insertCartridge(path.string());
        check(nes.isCartridgeLoaded() && !std::filesystem::exists(saves), "Loading created a battery file before changes");
        nes.getBus().cpuWrite(0x6000, 0xA5);
        nes.reset();
        check(nes.cpuRead(0x6000) == 0xA5, "NES reset erased battery RAM");
        std::stringstream snapshot;
        check(nes.saveState(snapshot), "Snapshot save failed");
        nes.getBus().cpuWrite(0x6000, 0xB6);
        check(nes.flushBattery(), "Explicit battery flush failed");
        check(fileBytes(save).size() == 8192 && fileBytes(save)[0] == 0xB6, "Battery raw file mismatch");
        const auto modified = std::filesystem::last_write_time(save);
        check(nes.flushBattery() && std::filesystem::last_write_time(save) == modified, "Unchanged battery was rewritten");
        check(nes.loadState(snapshot) && fileBytes(save)[0] == 0xB6, "Snapshot load wrote battery prematurely");
        check(nes.flushBattery() && fileBytes(save)[0] == 0xA5, "Restored NVRAM was not persisted on flush");
        nes.getBus().cpuWrite(0x6000, 0xC7);
        nes.insertCartridge(path.string());
        check(nes.cpuRead(0x6000) == 0xC7, "Same-ROM replacement loaded stale battery");
        auto held = nes.getBus().cart;
        nes.getBus().cpuWrite(0x6000, 0xD8);
        nes.unload();
        check(fileBytes(save)[0] == 0xD8, "Unload did not flush while cartridge remained referenced");
    }
    // Mesmo conteúdo em outro nome usa a mesma identidade de bateria.
    const auto renamed = root / "renamed.nes";
    std::filesystem::copy_file(path, renamed);
    std::shared_ptr<Cartridge> held;
    {
        NES nes(saves);
        nes.insertCartridge(renamed.string());
        check(nes.cpuRead(0x6000) == 0xD8, "Battery did not survive a new console/renamed ROM");
        held = nes.getBus().cart;
        nes.getBus().cpuWrite(0x6000, 0xE9);
    }
    check(fileBytes(save)[0] == 0xE9, "Console destruction did not flush battery");
    held.reset();
    {
        Cartridge cart(path.string());
        check(cart.loadBattery(saves), "Direct cartridge load failed");
        cart.cpuWrite(0x6000, 0xFA, 0);
    }
    check(fileBytes(save)[0] == 0xFA, "Cartridge destruction did not flush battery");

    const auto noBattery = root / "no-battery";
    std::filesystem::create_directories(noBattery);
    const auto volatilePath = saveRom(noBattery, rom(16, 0));
    {
        NES nes(noBattery / "saves");
        nes.insertCartridge(volatilePath.string());
        nes.getBus().cpuWrite(0x6000, 0xA5);
        check(nes.flushBattery(), "Volatile flush failed");
    }
    check(!std::filesystem::exists(noBattery / "saves"), "Volatile-only game created battery storage");

    // O header iNES de 16 KiB com bateria representa dois chips, só o segundo persistente.
    auto legacy = bytes;
    legacy[8] = 2;
    Cartridge sorom(saveRom(noBattery, legacy).string());
    check(sorom.isValid() && sorom.getMapper()->getBatteryData().size() == 8192, "Legacy SOROM persisted both chips");

    std::string error;
    std::vector<uint8_t> memory(8192, 0x5A);
    for (size_t invalidSize : {size_t(1), size_t(8191), size_t(8193)})
    {
        {
            std::ofstream output(save, std::ios::binary | std::ios::trunc);
            const std::string bad(invalidSize, 'x');
            output.write(bad.data(), bad.size());
        }
        const auto bad = fileBytes(save);
        check(BatterySave::load(save, memory, error) == BatterySave::LoadResult::Error && memory[0] == 0x5A,
              "Bad battery size was accepted or changed memory");
        {
            NES nes(saves);
            nes.insertCartridge(path.string());
            check(nes.isCartridgeLoaded(), "Bad battery prevented ROM loading");
            nes.getBus().cpuWrite(0x6000, 0x11);
            check(!nes.flushBattery(), "Failed battery load was silently overwritten");
        }
        check(fileBytes(save) == bad, "Bad battery file was destroyed");
    }
    // Falha na substituição deve deixar o destino existente intacto e remover o temporário.
    const auto blocked = root / "blocked";
    std::filesystem::create_directories(blocked);
    {
        std::ofstream sentinel(blocked / "keep");
        sentinel << "original";
    }
    check(!BatterySave::save(blocked, memory, error) && fileBytes(blocked / "keep").size() == 8,
          "Failed atomic replacement damaged destination");
    check(BatterySave::load(root / "absent.sav", memory, error) == BatterySave::LoadResult::Missing,
          "Missing battery treated as an error");
}
static void banking()
{
    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned bank = 0; bank < 32; ++bank)
        {
            Mapper001 m(16, 16, 1);
            uint32_t t = 0;
            writeRegister(m, 0x8000, (mode << 2) | 2, t);
            writeRegister(m, 0xE000, bank, t);
            for (uint16_t address : {0x8000, 0xBFFF, 0xC000, 0xFFFF})
            {
                unsigned b = mode < 2 ? (bank & 14) + (address >= 0xC000) : mode == 2 ? (address < 0xC000 ? 0 : bank & 15)
                                                                                      : (address < 0xC000 ? bank & 15 : 15);
                check(mapped(m, address) == b * 0x4000 + (address & 0x3FFF), "PRG mode/boundary failed");
            }
        }
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned b0 = 0; b0 < 32; ++b0)
            for (unsigned b1 = 0; b1 < 32; ++b1)
            {
                Mapper001 m(16, 16, 1);
                uint32_t t = 0, a = 0;
                uint8_t d = 0;
                writeRegister(m, 0x8000, (mode << 4) | 14, t);
                writeRegister(m, 0xA000, b0, t);
                writeRegister(m, 0xC000, b1, t);
                for (uint16_t address : {0, 0x0FFF, 0x1000, 0x1FFF})
                {
                    check(m.ppuMapRead(address, a, d, 0), "CHR read failed");
                    unsigned expected = mode ? (address < 0x1000 ? b0 : b1) * 0x1000 + (address & 0xFFF) : (b0 & 30) * 0x1000 + address;
                    check(a == expected, "CHR mode/boundary failed");
                    check(!m.ppuMapWrite(address, a, 0xA5, 0), "CHR ROM write accepted");
                }
            }
    for (unsigned mirror = 0; mirror < 4; ++mirror)
    {
        Mapper001 m(16, 0);
        uint32_t t = 0;
        writeRegister(m, 0x8000, 12 | mirror, t);
        check(static_cast<unsigned>(m.getMirrorMode()) == std::vector<unsigned>{2, 3, 1, 0}[mirror], "Mirroring failed");
    }
}

static void serialAndStates()
{
    {
        Mapper001 m(16, 0);
        uint32_t t = 0, a = 0;
        writeRegister(m, 0x8000, 0x11, t);
        m.cpuMapWrite(0x8000, a, 0x80, t + 4);
        check(static_cast<uint8_t>(state(m)[0]) == 0x1D && m.getMirrorMode() == MirrorMode::ONESCREEN_HI,
              "Serial reset changed CHR mode/mirroring");
        m.cpuMapWrite(0x6000, a, 0, t + 8);
        m.cpuMapWrite(0xE000, a, 1, t + 9);
        check(static_cast<uint8_t>(state(m)[5]) == 0, "Consecutive non-serial write not observed");
    }
    for (unsigned first : {0u, 1u, 0xFFFFFFFEu})
    {
        Mapper001 m(16, 0);
        uint32_t a = 0;
        check(m.cpuMapWrite(0xE000, a, 1, first), "First write not handled");
        check(static_cast<uint8_t>(state(m)[5]) == 1, "First write discarded");
        m.cpuMapWrite(0xE000, a, 0, first + 1);
        check(static_cast<uint8_t>(state(m)[5]) == 1, "Consecutive data accepted");
        m.cpuMapWrite(0xE000, a, 0x80, first + 2);
        check(static_cast<uint8_t>(state(m)[5]) == 0, "Consecutive reset discarded");
    }
    Mapper001 m(32, 0, 4);
    uint32_t t = 0, a = 0;
    writeRegister(m, 0x8000, 0x1E, t);
    writeRegister(m, 0xA000, 0, t);
    writeRegister(m, 0xC000, 0x1C, t);
    m.onPpuAddress(0x1000, 0);
    writeRegister(m, 0xE000, 3, t);
    check(mapped(m, 0xC000) == 31 * 0x4000, "Outer fixed bank wrong");
    m.cpuMapWrite(0x6000, a, 0xA5, t + 2);
    // Only the fifth address selects the register; the first four target Control.
    for (unsigned bit = 0; bit < 2; ++bit)
    {
        t += 4;
        m.cpuMapWrite(0x8000, a, (5 >> bit) & 1, t);
    }
    const auto valid = state(m);
    m.reset();
    std::stringstream saved(valid);
    m.loadState(saved);
    check(saved.good() && mapped(m, 0x8000) == 19 * 0x4000, "Partial serial state restore");
    uint8_t data = 0;
    check(m.cpuMapRead(0x6000, a, data) && data == 0xA5, "RAM restore failed");
    for (unsigned bit = 2; bit < 5; ++bit)
    {
        t += 4;
        m.cpuMapWrite(bit == 4 ? 0xFFFF : 0x8000, a, (5 >> bit) & 1, t);
    }
    check(mapped(m, 0x8000) == 21 * 0x4000, "Fifth address/serial restore failed");
    const auto unchanged = state(m);
    for (size_t length : {size_t(0), size_t(1), size_t(6), unchanged.size() - 1})
    {
        std::stringstream bad(unchanged.substr(0, length));
        m.loadState(bad);
        check(bad.fail() && state(m) == unchanged, "Truncated state mutated mapper");
    }
    for (unsigned field : {0, 1, 2, 3, 4, 5})
    {
        auto bytes = unchanged;
        bytes[field] = static_cast<char>(0xFF);
        std::stringstream bad(bytes);
        m.loadState(bad);
        check(bad.fail() && state(m) == unchanged, "Corrupt register/count accepted");
    }
    auto legacy = unchanged;
    legacy.back() &= ~0x40;
    std::stringstream old(legacy);
    m.loadState(old);
    check(old.good() && state(m) == unchanged, "Legacy mapper layout rejected");
    auto derived = unchanged;
    derived.back() ^= 1;
    std::stringstream stale(derived);
    m.loadState(stale);
    check(stale.good() && state(m) == unchanged, "Derived outer bank not recomputed");
}

static void cartridges(const std::filesystem::path &dir)
{
    for (unsigned sub : {1, 2, 4})
    {
        Cartridge deprecated(saveRom(dir, rom(sub == 2 ? 16 : 32, 0, sub == 1 ? 7 : sub == 2 ? 8
                                                                                             : 9,
                                              sub))
                                 .string());
        check(deprecated.isValid(), "Valid deprecated submapper rejected");
    }
    for (unsigned ram : {0, 7, 8, 9, 0x70, 0x77, 0x90})
    {
        Cartridge c(saveRom(dir, rom(32, 0, ram)).string());
        check(c.isValid(), "RAM configuration rejected");
        unsigned banks = ram == 0 ? 0 : ram == 0x77 ? 2
                                                    : 1u << ((ram & 15 ? ram & 15 : ram >> 4) - 7);
        uint32_t t = 0;
        writeRegister(*c.getMapper(), 0x8000, 0x0E, t);
        uint8_t data = 0;
        if (!banks)
        {
            check(!c.cpuRead(0x6000, data) && !c.cpuWrite(0x6000, 1, t), "Absent RAM exposed");
            continue;
        }
        for (unsigned bank = 0; bank < banks; ++bank)
        {
            writeRegister(*c.getMapper(), 0xA000, bank << (banks == 2 ? 3 : 2), t);
            check(c.cpuWrite(0x6000, 0x10 + bank, t + 2), "RAM write rejected");
        }
        for (unsigned bank = 0; bank < banks; ++bank)
        {
            writeRegister(*c.getMapper(), 0xA000, bank << (banks == 2 ? 3 : 2), t);
            check(read(c, 0x6000) == 0x10 + bank, "RAM banks alias");
        }
        writeRegister(*c.getMapper(), 0xE000, 16, t);
        check(!c.cpuRead(0x6000, data) && !c.cpuWrite(0x6000, 0, t + 2), "PRG RAM disable ignored");
    }
    for (unsigned chr : {0, 1})
    {
        Cartridge c(saveRom(dir, rom(32, chr)).string());
        uint32_t t = 0;
        writeRegister(*c.getMapper(), 0xA000, 16, t);
        check(read(c, 0xC000) == 31, "CHR ROM/RAM outer bank failed");
        Cartridge sn(saveRom(dir, rom(16, chr)).string());
        t = 0;
        uint8_t data = 0;
        writeRegister(*sn.getMapper(), 0xA000, 16, t);
        check(!sn.cpuRead(0x6000, data), "SNROM CHR.4 disable failed");
    }
    Cartridge sz(saveRom(dir, rom(16, 8, 0x77)).string());
    uint32_t t = 0;
    writeRegister(*sz.getMapper(), 0x8000, 14, t);
    writeRegister(*sz.getMapper(), 0xA000, 0, t);
    sz.cpuWrite(0x6000, 1, t + 2);
    writeRegister(*sz.getMapper(), 0xA000, 16, t);
    sz.cpuWrite(0x6000, 2, t + 2);
    writeRegister(*sz.getMapper(), 0xA000, 0, t);
    check(read(sz, 0x6000) == 1, "SZROM bank zero");
    writeRegister(*sz.getMapper(), 0xA000, 16, t);
    check(read(sz, 0x6000) == 2, "SZROM bank one");
    Cartridge fixed(saveRom(dir, rom(2, 2, 0, 5)).string());
    t = 0;
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        writeRegister(*fixed.getMapper(), 0x8000, mode << 2, t);
        writeRegister(*fixed.getMapper(), 0xE000, 1, t);
        check(read(fixed, 0x8000) == 0 && read(fixed, 0xFFFF) == 1, "Fixed 32 KiB PRG changed");
    }
    for (unsigned horizontal : {0, 1})
    {
        auto bytes = rom(16, 2, 0, 7);
        bytes[6] |= horizontal;
        Cartridge hard(saveRom(dir, bytes).string());
        t = 0;
        for (unsigned mirror = 0; mirror < 4; ++mirror)
        {
            writeRegister(*hard.getMapper(), 0x8000, 12 | mirror, t);
            check(hard.getMirrorMode() == (horizontal ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL), "Hardwired mirror changed");
        }
    }
    auto trained = rom(16, 0);
    trained[6] |= 4;
    trained.insert(trained.begin() + 16, 512, 0xA5);
    Cartridge trainer(saveRom(dir, trained).string());
    check(trainer.isValid() && read(trainer, 0x7000) == 0xA5, "Trainer not loaded");
    auto reject = [&](std::vector<uint8_t> bytes)
    { Cartridge c(saveRom(dir, bytes).string()); check(!c.isValid(), "Invalid MMC1 image accepted"); };
    for (unsigned sub : {3, 6, 8, 15})
        reject(rom(16, 0, 7, sub));
    reject(rom(16, 0, 7, 1));
    reject(rom(16, 0, 7, 2));
    reject(rom(16, 0, 7, 4));
    reject(rom(16, 2, 0, 5));
    for (unsigned size : {0, 3, 64})
        reject(rom(size, 0));
    reject(rom(16, 32));
    reject(rom(32, 2));
    reject(rom(16, 2, 9));
    reject(rom(16, 0, 0x78));
    auto bytes = rom(16, 0);
    bytes[11] = 6;
    reject(bytes);
    bytes = rom(16, 0);
    bytes[6] |= 8;
    reject(bytes);
    bytes = rom(16, 0);
    bytes.resize(16 + 15 * 0x4000);
    reject(bytes);
    bytes = rom(16, 2);
    bytes.pop_back();
    reject(bytes);
    bytes = rom(16, 0);
    bytes[7] = 0;
    bytes[8] = 0;
    bytes.resize(16 + 15 * 0x4000);
    reject(bytes);
    bytes = rom(16, 0);
    bytes[7] = 0;
    bytes[8] = 5;
    reject(bytes);
}

static std::string consoleState(NES &nes)
{
    std::stringstream s;
    check(nes.saveState(s), "Console save failed");
    return s.str();
}
static void consoleTests(const std::filesystem::path &dir)
{
    for (bool overclock : {false, true})
        for (unsigned original : {1, 0x7F, 0xFF})
        {
            auto bytes = rom(16, 0);
            const size_t base = 16 + 15 * 0x4000;
            bytes[base] = 0xEE;
            bytes[base + 1] = 0x10;
            bytes[base + 2] = 0xE0; // INC $E010
            bytes[base + 3] = 0x02;
            bytes[base + 0x2010] = original; // JAM after RMW.
            bytes[base + 0x3FFC] = 0;
            bytes[base + 0x3FFD] = 0xC0;
            NES nes;
            nes.insertCartridge(saveRom(dir, bytes).string());
            nes.setCPUOverclock(overclock);
            for (unsigned i = 0; i < 100; ++i)
                nes.step();
            const auto mapper = state(*nes.getBus().cart->getMapper());
            check(static_cast<uint8_t>(mapper[5]) == (original == 1 ? 1 : 0), "CPU RMW filter/reset failed");
            check(nes.getBus().cpuCycleCounter == nes.getSystemClockCounter() * (overclock ? 2u : 1u), "CPU clock not independent");
        }
    for (bool overclock : {false, true})
    {
        auto bytes = rom(16, 0);
        const size_t base = 16 + 15 * 0x4000;
        bytes[base] = 0x38;
        bytes[base + 1] = 0x7F; // SEC; RRA $E010,X (X starts at zero).
        bytes[base + 2] = 0x10;
        bytes[base + 3] = 0xE0;
        bytes[base + 4] = 0x02;
        bytes[base + 0x2010] = 0;
        bytes[base + 0x3FFC] = 0;
        bytes[base + 0x3FFD] = 0xC0;
        NES cpu;
        cpu.insertCartridge(saveRom(dir, bytes).string());
        cpu.setCPUOverclock(overclock);
        for (unsigned i = 0; i < 100; ++i)
            cpu.step();
        check(static_cast<uint8_t>(state(*cpu.getBus().cart->getMapper())[5]) == 0, "RRA second-write reset ignored");
    }
    NES nes;
    nes.insertCartridge(saveRom(dir, rom(16, 0)).string());
    auto &bus = nes.getBus();
    uint32_t t = 0;
    writeRegister(*bus.cart->getMapper(), 0xE000, 16, t);
    bus.cpuWrite(0x0000, 0xA5);
    check(bus.cpuRead(0x6000) == 0xA5 && bus.cpuRead(0x5000) == 0xA5, "Disabled RAM not open bus");
    check(bus.cpuRead(0xC000, true) == 15 && bus.cpuDataBus == 0xA5, "Debugger changed bus latch");
    check(bus.cpuRead(0xC000) == 15 && bus.cpuRead(0x6000) == 15, "ROM read did not drive open bus");
    const auto before = consoleState(nes);
    for (size_t length : {size_t(1), size_t(13), before.size() - 1})
    {
        std::stringstream broken(before.substr(0, length));
        check(!nes.loadState(broken) && consoleState(nes) == before, "Failed load changed console");
    }
    // The mapper payload begins after CPU/RAM/PPU/APU; derive it from the suffix.
    auto corrupt = before;
    std::stringstream chrState;
    bus.cart->saveState(chrState);
    const size_t mapperStart = before.size() - state(*bus.cart->getMapper()).size() - chrState.str().size();
    corrupt[mapperStart + 5] = 5;
    std::stringstream badMapper(corrupt);
    check(!nes.loadState(badMapper) && consoleState(nes) == before, "Bad mapper did not roll back console");
    bus.cpuWrite(0x0000, 0x11);
    bus.cpuCycleCounter = 123;
    std::stringstream restore(before);
    check(nes.loadState(restore) && consoleState(nes) == before, "New console state restore failed");
    for (uint8_t version : {0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A})
    {
        auto old = before;
        old[0] = version;
        if (version == 0x42)
            old.erase(8, 5); // Original R2NB header.
        std::stringstream legacy(old);
        check(!nes.loadState(legacy) && consoleState(nes) == before, "Legacy state accepted or changed console");
    }
}

static void nmiTiming(const std::filesystem::path &dir)
{
    // The game enables NMI with STA $2000, then stores the same value in
    // its RAM shadow. An edge on STA's last cycle must let that store run.
    for (unsigned edgeCycle = 0; edgeCycle <= 4; ++edgeCycle)
    {
        auto bytes = rom(8, 16);
        const size_t base = 16 + 7 * 0x4000;
        const uint8_t main[] = {0xA9, 0x80, 0x8D, 0x00, 0x20, 0x85, 0x00, 0x02};
        const uint8_t handler[] = {0x48, 0xA5, 0x00, 0x85, 0x01, 0x68, 0x40};
        std::copy(std::begin(main), std::end(main), bytes.begin() + base);
        std::copy(std::begin(handler), std::end(handler), bytes.begin() + base + 0x100);
        bytes[base + 0x3FFA] = 0;
        bytes[base + 0x3FFB] = 0xC1;
        bytes[base + 0x3FFC] = 0;
        bytes[base + 0x3FFD] = 0xC0;
        NES nes;
        nes.insertCartridge(saveRom(dir, bytes).string());
        for (unsigned i = 0; i < 9; ++i)
            nes.step(); // Reset + LDA #$80.
        check(nes.getCpu().complete() && nes.getCpu().pc == 0xC002, "NMI fixture setup failed");
        if (edgeCycle == 0)
        {
            // Reach the second VBlank with the CPU paused, past the PPU's
            // power-up write lockout. STA $2000 now produces the real edge.
            for (unsigned frame = 0; frame < 2; ++frame)
            {
                while (nes.cpuRead(0x2002) & 0x80)
                    nes.getPpu().clock();
                while (!(nes.cpuRead(0x2002) & 0x80))
                    nes.getPpu().clock();
            }
            for (unsigned i = 0; i < 4; ++i)
                nes.step();
        }
        else
        {
            for (unsigned i = 0; i < edgeCycle; ++i)
                nes.step();
            nes.getCpu().nmi();
        }
        const auto pending = consoleState(nes);
        for (unsigned replay = 0; replay < 2; ++replay)
        {
            if (replay)
            {
                std::stringstream saved(pending);
                check(nes.loadState(saved), "Pending NMI restore failed");
            }
            for (unsigned i = 0; i < 100; ++i)
                nes.step();
            check(nes.cpuRead(0) == 0x80, "NMI lost RAM shadow store");
            check(nes.cpuRead(1) == (edgeCycle == 0 || edgeCycle == 4 ? 0x80 : 0), "NMI accepted at wrong instruction boundary");
        }
    }
}

static void gameBoot(const char *path)
{
    NES nes;
    nes.insertCartridge(path);
    check(nes.isCartridgeLoaded(), "Game ROM load failed");
    std::vector<uint32_t> colors;
    unsigned frames = 0, interruptEntries = 0;
    const uint16_t vector = nes.cpuRead(0xFFFA) | (nes.cpuRead(0xFFFB) << 8);
    for (unsigned i = 0; i < 15000000; ++i)
    {
        if (nes.getCpu().complete() && nes.getCpu().pc == vector)
            ++interruptEntries;
        nes.step();
        if (nes.isFrameComplete())
        {
            ++frames;
            nes.clearFrameComplete();
            const auto pixels = nes.getPpu().getFrameBuffer();
            for (unsigned p = 0; p < 256 * 240; ++p)
                if (std::find(colors.begin(), colors.end(), pixels[p]) == colors.end())
                    colors.push_back(pixels[p]);
        }
    }
    std::cout << "Game boot: frames=" << frames << ", NMI entries=" << interruptEntries
              << ", colors=" << colors.size() << '\n';
    check(interruptEntries > 100 && colors.size() > 4, "Game boot stalled or did not render");
}

int main(int argc, char **argv)
{
    try
    {
        check(argc >= 2, "Pass a directory for generated ROMs and optional game ROM");
        std::filesystem::create_directories(argv[1]);
        battery(argv[1]);
        banking();
        serialAndStates();
        cartridges(argv[1]);
        consoleTests(argv[1]);
        nmiTiming(argv[1]);
        if (argc >= 3)
            gameBoot(argv[2]);
        std::cout << "MMC1 tests passed (banking, RAM, submappers, serial/RMW, overclock, states, open bus)\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
