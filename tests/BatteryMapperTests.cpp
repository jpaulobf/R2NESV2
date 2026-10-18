#include "Core/NES.h"
#include "Core/Cartridge/Mappers/Mapper004.h"
#include "Core/Cartridge/Mappers/Mapper005.h"
#include "Core/Cartridge/Mappers/Mapper009.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static std::string state(Mapper &mapper)
{
    std::stringstream stream;
    mapper.saveState(stream);
    return stream.str();
}
static void write(Mapper &mapper, uint16_t addr, uint8_t value)
{
    uint32_t mapped = 0;
    mapper.cpuMapWrite(addr, mapped, value, 0);
}
static uint8_t read(Mapper &mapper, uint16_t addr)
{
    uint32_t mapped = 0;
    uint8_t value = 0;
    check(mapper.cpuMapRead(addr, mapped, value) && mapped == 0xFFFFFFFF, "RAM read escaped to ROM");
    return value;
}
static void enable(Mapper &mapper, unsigned id, bool mmc6 = false)
{
    if (id == 4)
    {
        write(mapper, 0x8000, mmc6 ? 0x20 : 0);
        write(mapper, 0xA001, mmc6 ? 0xF0 : 0x80);
    }
    if (id == 5)
    {
        write(mapper, 0x5102, 2);
        write(mapper, 0x5103, 1);
        write(mapper, 0x5104, 2);
    }
}
static void mapperContracts()
{
    for (bool mmc6 : {false, true})
    {
        Mapper004::Configuration config;
        config.submapper = mmc6 ? 1 : 0;
        config.prgRamSize = 0;
        config.prgNvRamSize = mmc6 ? 1024 : 8192;
        Mapper004 mapper(2, 1, MirrorMode::HORIZONTAL, config);
        enable(mapper, 4, mmc6);
        const uint16_t base = mmc6 ? 0x7000 : 0x6000;
        write(mapper, base, 0xA5);
        write(mapper, base + config.prgNvRamSize - 1, 0xB6);
        write(mapper, 0xA001, 0); // Proteção de CPU não deve impedir persistência física.
        const auto bytes = mapper.getBatteryData();
        check(bytes.size() == config.prgNvRamSize && bytes.front() == 0xA5 && bytes.back() == 0xB6, "MMC3/MMC6 battery size/data mismatch");
        const auto before = state(mapper);
        check(!mapper.setBatteryData({1}) && state(mapper) == before, "MMC3/MMC6 bad import changed state");
        mapper.reset();
        check(mapper.getBatteryData() == bytes, "MMC3/MMC6 reset erased NVRAM");
        std::vector<uint8_t> imported(bytes.size(), 0xC7);
        check(mapper.setBatteryData(imported), "MMC3/MMC6 import failed");
        enable(mapper, 4, mmc6);
        check(read(mapper, base) == 0xC7, "MMC3/MMC6 physical import mapping mismatch");
    }
    Mapper004 volatileMmc3(2, 1, MirrorMode::HORIZONTAL);
    check(volatileMmc3.getBatteryData().empty(), "Volatile MMC3 became battery backed");
    for (unsigned nvram : {8192, 32768})
        for (unsigned ram : {8192, 32768})
        {
            Mapper005::Configuration config;
            config.prgRamSize = ram;
            config.prgNvRamSize = nvram;
            Mapper005 mapper(2, 1, config);
            enable(mapper, 5);
            write(mapper, 0x5113, 0);
            write(mapper, 0x6000, 0xA5);
            write(mapper, 0x5113, 4);
            write(mapper, 0x6000, 0xB6);
            write(mapper, 0x5C00, 0xC7);
            const auto bytes = mapper.getBatteryData();
            check(bytes.size() == nvram + 1024 && bytes[0] == 0xA5 && bytes[nvram] == 0xC7, "MMC5 persisted wrong chip or omitted ExRAM");
            const auto before = state(mapper);
            check(!mapper.setBatteryData({1}) && state(mapper) == before, "MMC5 partial import mutated state");
            std::vector<uint8_t> imported(bytes.size(), 0xD8);
            check(mapper.setBatteryData(imported) && read(mapper, 0x6000) == 0xB6, "MMC5 import overwrote volatile second chip");
            write(mapper, 0x5113, 0);
            check(read(mapper, 0x6000) == 0xD8 && read(mapper, 0x5C00) == 0xD8, "MMC5 import missed first chip/ExRAM");
            mapper.reset();
            check(mapper.getBatteryData() == imported, "MMC5 reset erased battery data");
        }
    Mapper005 volatileMmc5(2, 1);
    check(volatileMmc5.getBatteryData().empty(), "Volatile MMC5 created battery data");
    Mapper005::Configuration internal;
    internal.prgRamSize = 0;
    internal.batteryBackedExRam = true;
    Mapper005 onlyExRam(2, 1, internal);
    check(onlyExRam.getBatteryData().size() == 1024, "Internal-only ExRAM battery omitted");
    const auto original = state(volatileMmc5);
    Mapper005::Configuration backed;
    backed.batteryBackedExRam = true;
    Mapper005 incompatible(2, 1, backed);
    const auto before = state(incompatible);
    std::stringstream noBattery(original);
    incompatible.loadState(noBattery);
    check(noBattery.fail() && state(incompatible) == before, "ExRAM battery configuration mismatch accepted in snapshot");

    Mapper009::Configuration external;
    external.prgNvRamSize = 8192;
    Mapper009 mmc2(2, 1, MirrorMode::HORIZONTAL, external), standard(2, 1, MirrorMode::HORIZONTAL);
    write(mmc2, 0x6000, 0xA5);
    write(mmc2, 0x7FFF, 0xB6);
    const auto bytes = mmc2.getBatteryData();
    check(bytes.size() == 8192 && bytes.front() == 0xA5 && bytes.back() == 0xB6, "MMC2 external battery mapping mismatch");
    const auto saved = state(mmc2);
    check(!mmc2.setBatteryData({1}) && state(mmc2) == saved, "MMC2 partial battery import changed memory");
    write(mmc2, 0x6000, 0);
    std::stringstream restore(saved);
    mmc2.loadState(restore);
    check(restore.good() && read(mmc2, 0x6000) == 0xA5, "MMC2 external RAM missing from snapshot");
    const auto absent = state(standard);
    std::stringstream wrong(saved);
    standard.loadState(wrong);
    check(wrong.fail() && state(standard) == absent && standard.getBatteryData().empty(), "MMC2 state enabled phantom RAM");
}
static std::vector<uint8_t> image(unsigned mapper, uint8_t memory, uint8_t sub = 0)
{
    std::vector<uint8_t> bytes(16 + 32768 + 8192, 0);
    bytes[0] = 'N';
    bytes[1] = 'E';
    bytes[2] = 'S';
    bytes[3] = 0x1A;
    bytes[4] = 2;
    bytes[5] = 1;
    bytes[6] = (mapper << 4) | (memory >> 4 ? 2 : 0);
    bytes[7] = 8;
    bytes[8] = sub << 4;
    bytes[10] = memory;
    return bytes;
}
static std::filesystem::path saveRom(const std::filesystem::path &root, const std::vector<uint8_t> &bytes)
{
    const auto path = root / "fixture.nes";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    return path;
}
static void sessions(const std::filesystem::path &root)
{
    for (unsigned id : {4, 5, 9})
        for (bool special : {false, true})
        {
            if (id == 9 && special)
                continue;
            const uint8_t memory = id == 4 && special ? 0x40 : id == 5 && special ? 0x77
                                                                                  : 0x70;
            const auto caseDir = root / (std::to_string(id) + (special ? "-special" : "-normal"));
            std::filesystem::create_directories(caseDir);
            const auto path = saveRom(caseDir, image(id, memory, id == 4 && special ? 1 : 0));
            const auto saves = caseDir / "battery";
            const unsigned expected = id == 4 && special ? 1024 : id == 5 ? 9216
                                                                          : 8192;
            std::filesystem::path persisted;
            const uint16_t address = id == 4 && special ? 0x7000 : 0x6000;
            {
                NES nes(saves);
                nes.insertCartridge(path.string());
                check(nes.isCartridgeLoaded(), "Battery fixture rejected");
                auto &mapper = *nes.getBus().cart->getMapper();
                enable(mapper, id, special);
                write(mapper, address, 0xA5);
                if (id == 5)
                {
                    write(mapper, 0x5C00, 0xC7);
                    if (special)
                    {
                        write(mapper, 0x5113, 4);
                        write(mapper, 0x6000, 0xB6);
                    }
                }
                persisted = saves / (nes.getBus().cart->getRomHash() + ".sav");
            }
            check(std::filesystem::file_size(persisted) == expected, "Battery file includes volatile memory or wrong capacity");
            {
                NES nes(saves);
                nes.insertCartridge(path.string());
                auto &mapper = *nes.getBus().cart->getMapper();
                enable(mapper, id, special);
                check(read(mapper, address) == 0xA5, "Fresh session did not restore NVRAM");
                if (id == 5)
                {
                    check(read(mapper, 0x5C00) == 0xC7, "Fresh session lost battery-backed ExRAM");
                    if (special)
                    {
                        write(mapper, 0x5113, 4);
                        check(read(mapper, 0x6000) == 0, "Volatile MMC5 chip survived session");
                    }
                }
            }
        }
    for (unsigned id : {4, 5, 9})
    {
        const auto path = saveRom(root, image(id, 7));
        Cartridge cart(path.string());
        check(cart.isValid() && cart.getMapper()->getBatteryData().empty(), "Volatile header enabled battery");
        check(cart.loadBattery(root / "unused") && cart.flushBattery() && !std::filesystem::exists(root / "unused"), "Volatile game created a battery directory");
    }
    for (uint8_t units : {0, 1, 2, 4, 8, 16})
    {
        auto bytes = image(5, 0x70);
        bytes[7] = 0;
        bytes[8] = units;
        Cartridge cart(saveRom(root, bytes).string());
        check(cart.isValid(), "Legacy MMC5 battery header rejected");
        const unsigned total = units ? units * 8192 : 65536;
        const unsigned nv = total == 16384 || total == 65536 ? total / 2 : total;
        check(cart.getMapper()->getBatteryData().size() == nv + 1024, "Legacy MMC5 persisted wrong number of chips");
    }
    auto legacy = image(9, 0x70);
    legacy[7] = 0;
    legacy[8] = 0;
    Cartridge battery(saveRom(root, legacy).string());
    check(battery.isValid() && battery.getMapper()->getBatteryData().size() == 8192, "Legacy MMC2 battery extension rejected");
    for (uint8_t ram : {6, 8, 0x60, 0x80, 0x77})
    {
        Cartridge invalid(saveRom(root, image(9, ram)).string());
        check(!invalid.isValid(), "Invalid MMC2 RAM geometry accepted");
    }
    auto contradictory = image(9, 0);
    contradictory[6] |= 2;
    Cartridge invalid(saveRom(root, contradictory).string());
    check(!invalid.isValid(), "NES 2.0 MMC2 battery without NVRAM accepted");
}
static std::string consoleState(NES &nes)
{
    std::stringstream stream;
    check(nes.saveState(stream), "Console save failed");
    return stream.str();
}
static void snapshotIntegrity(const std::filesystem::path &root)
{
    auto bytes = image(5, 7);
    bytes[16] = 0x4C;
    bytes[17] = 0;
    bytes[18] = 0x80; // JMP $8000.
    bytes[16 + 32764] = 0;
    bytes[16 + 32765] = 0x80;
    const auto path = saveRom(root, bytes);
    for (unsigned prefix : {0, 1, 2, 7, 257})
    {
        NES nes(std::filesystem::path{});
        nes.insertCartridge(path.string());
        for (unsigned i = 0; i < 7; ++i)
            nes.step();
        auto &bus = nes.getBus();
        for (unsigned i = 0; i < 256; ++i)
            bus.cpuWrite(0x0200 + i, (i * 17) ^ 0xA5);
        bus.cpuWrite(0x4014, 2);
        for (unsigned i = 0; i < prefix; ++i)
            nes.step();
        check(bus.dma_transfer, "DMA fixture finished too early");
        const auto pending = consoleState(nes);
        const auto page = bus.dma_page, address = bus.dma_addr, data = bus.dma_data;
        const bool dummy = bus.dma_dummy;
        unsigned steps = 0;
        while (bus.dma_transfer && steps < 600)
        {
            nes.step();
            ++steps;
        }
        check(!bus.dma_transfer && steps < 600, "DMA never finished");
        for (unsigned i = 0; i < 3; ++i)
            nes.step();
        const auto completed = consoleState(nes);
        std::stringstream restore(pending);
        check(nes.loadState(restore), "Active DMA snapshot rejected");
        check(bus.dma_transfer && bus.dma_page == page && bus.dma_addr == address &&
                  bus.dma_data == data && bus.dma_dummy == dummy,
              "DMA control was not restored after completion");
        for (unsigned i = 0; i < steps + 3; ++i)
            nes.step();
        check(consoleState(nes) == completed, "DMA restore changed future CPU/PPU/OAM execution");
        for (size_t flag : {size_t(26), size_t(27)})
        {
            auto invalid = pending;
            invalid[flag] = 2;
            std::stringstream input(invalid);
            check(!nes.loadState(input) && consoleState(nes) == completed, "Invalid DMA flag changed console");
        }
        for (size_t length = 0; length < 28; ++length)
        {
            std::stringstream shortInput(pending.substr(0, length));
            check(!nes.loadState(shortInput) && consoleState(nes) == completed, "Truncated identity/DMA header changed console");
        }
        // Falha semântica posterior à leitura da DMA também deve restaurar o barramento.
        std::stringstream suffix;
        bus.cart->saveState(suffix);
        const size_t mapperStart = pending.size() - state(*bus.cart->getMapper()).size() - suffix.str().size();
        auto invalid = pending;
        invalid[mapperStart] = 4;
        std::stringstream badMapper(invalid);
        check(!nes.loadState(badMapper) && consoleState(nes) == completed && !bus.dma_transfer,
              "Failed mapper load did not roll back DMA");
    }
    NES first(std::filesystem::path{});
    first.insertCartridge(path.string());
    const auto original = consoleState(first);
    const auto otherDir = root / "other";
    std::filesystem::create_directories(otherDir);
    auto changed = bytes;
    changed[16 + 100] ^= 0x5A;
    NES second(std::filesystem::path{});
    second.insertCartridge(saveRom(otherDir, changed).string());
    const auto untouched = consoleState(second);
    std::stringstream wrongRom(original);
    check(!second.loadState(wrongRom) && consoleState(second) == untouched, "Same-capacity different ROM state accepted");
    changed = bytes;
    changed[6] = 0x40;
    NES otherMapper(std::filesystem::path{});
    otherMapper.insertCartridge(saveRom(otherDir, changed).string());
    check(otherMapper.getBus().cart->getRomHash() == first.getBus().cart->getRomHash(), "Mapper identity fixture changed payload");
    const auto mapperBefore = consoleState(otherMapper);
    std::stringstream wrongBoard(original);
    check(!otherMapper.loadState(wrongBoard) && consoleState(otherMapper) == mapperBefore, "Same payload different mapper state accepted");
}
static void imports(const std::filesystem::path &root)
{
    const auto readFile = [](const std::filesystem::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
    };
    const auto writeFile = [](const std::filesystem::path &path, const std::vector<uint8_t> &data)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(data.data()), data.size());
        check(output.good(), "Import fixture write failed");
    };
    for (unsigned kind : {0, 1, 2, 3, 4})
    {
        const auto dir = root / ("import-" + std::to_string(kind));
        std::filesystem::create_directories(dir);
        const unsigned targetNv = kind == 1 ? 32768 : 8192;
        const auto path = saveRom(dir, image(5, targetNv == 32768 ? 0x90 : 0x70));
        Cartridge cart(path.string());
        const auto save = dir / (cart.getRomHash() + ".sav");
        std::vector<uint8_t> source(kind >= 2 ? 32768 + 1024 : 8192, 0x5A);
        if (kind >= 2)
        {
            std::fill(source.end() - 1024, source.end(), 0xC7);
            if (kind == 3)
                source[9000] = 0xB6; // Dados distintos impedem reduzir bancos.
        }
        writeFile(save, source);
        const auto before = state(*cart.getMapper());
        check(!cart.loadBattery(dir) && cart.hasBatteryLoadError() && state(*cart.getMapper()) == before,
              "Import proposal changed emulated memory");
        if (kind == 3)
        {
            check(!cart.canImportBattery() && !cart.importBattery() && readFile(save) == source,
                  "Ambiguous shrink discarded data");
            continue;
        }
        check(cart.canImportBattery() && !cart.getBatteryMessage().empty() && !cart.flushBattery() && readFile(save) == source,
              "Recognized import was applied without explicit request");
        if (kind == 4)
        {
            writeFile(save, {1});
            check(!cart.importBattery() && state(*cart.getMapper()) == before && readFile(save) == std::vector<uint8_t>{1},
                  "Import used a stale file proposal");
            continue;
        }
        auto backup = save;
        backup += ".bak";
        if (kind == 1)
            writeFile(backup, {0x11}); // Um backup anterior não pode ser sobrescrito.
        check(cart.importBattery() && !cart.hasBatteryLoadError(), "Explicit MMC5 import failed");
        if (kind == 1)
        {
            check(readFile(backup) == std::vector<uint8_t>{0x11}, "Previous backup overwritten");
            backup += ".1";
        }
        check(readFile(backup) == source, "Original import file not preserved exactly");
        const auto imported = cart.getMapper()->getBatteryData();
        check(readFile(save) == imported && imported.size() == targetNv + 1024 && imported[0] == 0x5A,
              "Imported memory/file mismatch");
        check(imported[targetNv] == (kind >= 2 ? 0xC7 : 0), "Import ExRAM initialization mismatch");
        if (targetNv > 8192)
            check(imported[8192] == 0, "Expanded NVRAM not initialized");
        Cartridge fresh(path.string());
        check(fresh.loadBattery(dir) && fresh.getMapper()->getBatteryData() == imported, "Converted battery failed normal reload");
    }
}
int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass generated fixtures directory");
        const auto root = std::filesystem::path(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        mapperContracts();
        sessions(root);
        snapshotIntegrity(root);
        imports(root);
        std::cout << "Battery tests passed (MMC3/MMC6, MMC5/ExRAM, explicit MMC2 expansion)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
