#include "Core/NES.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace R2NES::Core;
static void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}
static std::string save(NES &nes)
{
    std::ostringstream out;
    check(nes.saveState(out), "save failed");
    return out.str();
}
static void load(NES &nes, const std::string &bytes)
{
    std::istringstream in(bytes);
    check(nes.loadState(in), "valid snapshot rejected");
}
static std::vector<float> pendingAudio(NES &nes)
{
    auto apu = nes.getApu();
    std::vector<float> samples;
    while (apu.hasSamples())
        samples.push_back(apu.getOutputSample());
    return samples;
}
template <typename T>
static std::string component(T &value)
{
    std::ostringstream out;
    value.saveState(out);
    check(bool(out), "component save failed");
    return out.str();
}

struct Layout
{
    size_t cpu, ppu, apu, mapper, end;
};
static Layout layout(NES &nes, const std::string &snapshot)
{
    const size_t cpu = 4 + 8 + 2 + 2 * sizeof(uint32_t) + 1 + 5;
    const auto cpuBytes = component(nes.getCpu());
    const size_t ppu = cpu + cpuBytes.size() + 2048;
    const auto ppuBytes = component(nes.getPpu());
    const size_t apu = ppu + ppuBytes.size();
    const auto apuBytes = component(nes.getApu());
    const size_t mapper = apu + apuBytes.size();
    const auto mapperBytes = component(*nes.getBus().cart->getMapper());
    check(snapshot.compare(cpu, cpuBytes.size(), cpuBytes) == 0 &&
              snapshot.compare(ppu, ppuBytes.size(), ppuBytes) == 0 &&
              snapshot.compare(apu, apuBytes.size(), apuBytes) == 0 &&
              snapshot.compare(mapper, mapperBytes.size(), mapperBytes) == 0 &&
              mapper + mapperBytes.size() == snapshot.size(),
          "unexpected snapshot layout");
    return {cpu, ppu, apu, mapper, snapshot.size()};
}

static std::pair<int16_t, int16_t> position(NES &nes)
{
    const auto bytes = component(nes.getPpu());
    constexpr size_t scanlineOffset = 2048 + 2 + 4 + 1 + 4 + 2 + 2 + 1 + 1 + 1 + 32 + 4;
    int16_t line = 0, dot = 0;
    check(bytes.size() >= scanlineOffset + 4, "PPU state too small");
    std::memcpy(&line, bytes.data() + scanlineOffset, 2);
    std::memcpy(&dot, bytes.data() + scanlineOffset + 2, 2);
    return {line, dot};
}

static void verifyScenario(NES &live, const std::filesystem::path &rom, const std::string &name)
{
    const auto baseline = save(live);
    const auto offsets = layout(live, baseline);
    struct Sample
    {
        std::string name, bytes;
    };
    std::vector<Sample> invalid;
    const auto truncate = [&](const char *label, size_t length)
    { invalid.push_back({label, baseline.substr(0, length)}); };
    truncate("header", 5);
    truncate("CPU", offsets.cpu + 3);
    truncate("RAM", offsets.cpu + component(live.getCpu()).size() + 100);
    truncate("PPU", offsets.ppu + 100);
    truncate("APU", offsets.apu + 10);
    truncate("mapper", offsets.end - 1);
    const auto mutate = [&](const char *label, size_t offset, uint8_t value)
    { auto bytes = baseline; check(offset < bytes.size(), "mutation out of range"); bytes[offset] = char(value); invalid.push_back({label, std::move(bytes)}); };
    mutate("DMA flag", offsets.cpu - 2, 2);
    mutate("CPU flag", offsets.cpu + 25, 2);
    mutate("PPU fine X", offsets.ppu + 2063, 255);
    mutate("APU duty", offsets.apu + 32, 255);
    mutate("MMC3 register", offsets.mapper, 255);

    const auto audioBefore = pendingAudio(live);
    const bool frameBefore = live.isFrameComplete();
    for (const auto &sample : invalid)
    {
        std::istringstream liveBroken(sample.bytes);
        check(!live.loadState(liveBroken) && save(live) == baseline, name + ": live rollback failed for " + sample.name);
        check(pendingAudio(live) == audioBefore, name + ": failed load changed pending audio for " + sample.name);
        check(live.isFrameComplete() == frameBefore, name + ": failed load changed frame completion");
    }
    const bool oldIrq = live.getCpu().isIrqLineAsserted();
    live.getCpu().setIrqLine(!oldIrq);
    std::istringstream badIrq(invalid.back().bytes);
    check(!live.loadState(badIrq) && live.getCpu().isIrqLineAsserted() != oldIrq,
          name + ": failed load changed CPU IRQ line");
    live.getCpu().setIrqLine(oldIrq);
    NES control("");
    control.setCheatDirectory(rom.parent_path() / "no-cheats");
    check(control.insertCartridge(rom.string()), "control cartridge rejected");
    load(control, baseline);
    for (const auto &sample : invalid)
    {
        NES candidate("");
        candidate.setCheatDirectory(rom.parent_path() / "no-cheats");
        check(candidate.insertCartridge(rom.string()), "candidate cartridge rejected");
        load(candidate, baseline);
        std::istringstream broken(sample.bytes);
        check(!candidate.loadState(broken) && broken.fail(), name + ": accepted " + sample.name);
        check(save(candidate) == baseline, name + ": rollback mismatch for " + sample.name);
        NES expected("");
        expected.setCheatDirectory(rom.parent_path() / "no-cheats");
        check(expected.insertCartridge(rom.string()), "reference cartridge rejected");
        load(expected, baseline);
        for (int step = 0; step < 700; ++step)
        {
            candidate.step();
            expected.step();
            if (step == 0 || step == 11 || step == 699)
            {
                check(save(candidate) == save(expected), name + ": continuation mismatch for " + sample.name);
                check(pendingAudio(candidate) == pendingAudio(expected), name + ": audio continuation mismatch for " + sample.name);
            }
        }
    }
    for (int i = 0; i < 700; ++i)
    {
        live.step();
        control.step();
    }
    check(save(live) == save(control), name + ": valid restore changed future execution");
    std::cout << name << ": " << invalid.size() << " malformed states checked\n";
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "fixture directory required");
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        const auto path = dir / "timing-mmc3.nes";
        std::vector<uint8_t> rom(16 + 32768 + 8192, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1a;
        rom[4] = 2;
        rom[5] = 1;
        rom[6] = 0x40;
        const uint8_t program[] = {0xa9, 0x02, 0x8d, 0x14, 0x40, 0x4c, 0x00, 0x80};
        std::copy(std::begin(program), std::end(program), rom.begin() + 16);
        for (size_t p : {size_t(0x7ffa), size_t(0x7ffc), size_t(0x7ffe)})
        {
            rom[16 + p] = 0;
            rom[16 + p + 1] = 0x80;
        }
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        check(bool(file), "fixture write failed");
        file.close();
        NES nes("");
        nes.setCheatDirectory(dir / "no-cheats");
        check(nes.insertCartridge(path.string()), "fixture ROM rejected");
        nes.reset();
        for (int i = 0; i < 256; ++i)
            nes.getBus().cpuWrite(0x0200 + i, uint8_t(i ^ 0x5a));
        for (int limit = 0; limit < 100 && !(nes.getCpu().pc == 0x8005 && !nes.getCpu().complete()); ++limit)
            nes.step();
        check(nes.getCpu().pc == 0x8005 && !nes.getCpu().complete() && !nes.getBus().dma_transfer, "did not reach STA mid-instruction");
        verifyScenario(nes, path, "CPU mid-instruction");
        for (int i = 0; i < 5000 && (!nes.getBus().dma_transfer || nes.getBus().dma_dummy || nes.getBus().dma_addr < 42); ++i)
            nes.step();
        check(nes.getBus().dma_transfer && !nes.getBus().dma_dummy && nes.getBus().dma_addr >= 42, "DMA checkpoint not reached");
        check(nes.getBus().dma_addr < 200, "DMA checkpoint not intermediate");
        check(!pendingAudio(nes).empty(), "DMA checkpoint did not contain pending audio");
        const auto dmaCheckpoint = save(nes);
        const auto dmaClock = nes.getSystemClockCounter();
        verifyScenario(nes, path, "OAM DMA first phase");
        for (int i = 0; i < 256; ++i)
            check(nes.getPpu().getOamMemory()[i] == uint8_t(i ^ 0x5a), "OAM DMA produced incorrect bytes");
        load(nes, dmaCheckpoint);
        nes.step();
        check(nes.getBus().dma_transfer && ((nes.getSystemClockCounter() ^ dmaClock) & 1),
              "DMA second phase not reached");
        verifyScenario(nes, path, "OAM DMA second phase");
        for (int i = 0; i < 1000 && nes.getBus().dma_transfer; ++i)
            nes.step();
        check(!nes.getBus().dma_transfer, "OAM DMA did not finish");
        nes.getBus().cpuWrite(0x4010, 0x8f);
        nes.getBus().cpuWrite(0x4012, 0);
        nes.getBus().cpuWrite(0x4013, 0);
        nes.getBus().cpuWrite(0x4015, 0x10);
        check(nes.getApu().hasDmcDmaRequest(), "DMC request not raised");
        nes.step();
        auto probe = nes.getApu();
        check(probe.consumeDmcDmaStallCycle(), "DMC stall checkpoint not reached");
        verifyScenario(nes, path, "DMC DMA stall");
        while (nes.getSystemClockCounter() < 35000)
            nes.step();
        nes.getPpu().cpuWrite(0x2001, 0x1e);
        bool visible = false;
        for (int i = 0; i < 40000; ++i)
        {
            nes.step();
            if (i % 16 == 0)
            {
                const auto [line, dot] = position(nes);
                if (line >= 10 && line < 100 && dot >= 80 && dot < 220)
                {
                    visible = true;
                    break;
                }
            }
        }
        check(visible, "visible rendering checkpoint not reached");
        check(nes.isFrameComplete(), "completed-frame flag not exercised");
        const auto ppuBytes = component(nes.getPpu());
        check((uint8_t(ppuBytes[2049]) & 0x18) == 0x18 && (uint8_t(ppuBytes[2054]) & 0x18) == 0x18,
              "rendering was not enabled at checkpoint");
        verifyScenario(nes, path, "PPU rendering");
        std::cout << "Intermediate-state rollback and continuation passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}