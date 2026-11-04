#include "Core/APU/APU.h"
#include "Core/CPU/CPU.h"
#include "Core/PPU/PPU.h"
#include "Core/NES.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<typename T> static std::string saved(T &device)
{ std::ostringstream out; device.saveState(out); check(bool(out), "save failed"); return out.str(); }

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "fixture directory required");
        APU a, b;
        a.cpuWrite(0x4015, 1); a.cpuWrite(0x4002, 40); a.cpuWrite(0x4003, 8);
        b.cpuWrite(0x4015, 1); b.cpuWrite(0x4002, 40); b.cpuWrite(0x4003, 8);
        b.cpuWrite(0x4000, 0xc0);
        auto state = saved(a), changed = saved(b);
        size_t mode = state.size(), diffs = 0;
        for (size_t i = 0; i < state.size(); ++i)
            if (state[i] != changed[i]) { mode = i; ++diffs; }
        check(diffs == 1, "Could not isolate pulse duty mode in state");
        state[mode] = char(255);
        std::istringstream apuInput(state); a.loadState(apuInput);
        check(apuInput.fail(), "APU accepted out-of-range duty mode");
        state = saved(b);
        state[sizeof(uint32_t) + sizeof(uint8_t)] = char(2); // apuCyclePhase
        std::istringstream apuFlag(state); b.loadState(apuFlag);
        check(apuFlag.fail(), "APU accepted invalid bool representation");
        APU cleanApu;
        const auto apuBaseline = saved(cleanApu);
        auto rejectApuByte = [&](size_t offset, uint8_t value, const char *message)
        {
            auto bytes = apuBaseline;
            check(offset < bytes.size(), "APU mutation offset");
            bytes[offset] = static_cast<char>(value);
            APU target;
            std::istringstream input(bytes); target.loadState(input);
            check(input.fail(), message);
        };
        rejectApuByte(4, 3, "APU accepted invalid frame mode");
        rejectApuByte(9, 6, "APU accepted invalid frame-write delay");
        rejectApuByte(10, 3, "APU accepted invalid pending mode");
        rejectApuByte(39, 16, "APU accepted invalid envelope volume");
        rejectApuByte(45, 8, "APU accepted invalid sweep shift");
        rejectApuByte(46, 8, "APU accepted invalid sweep timer");
        auto invalidClock = apuBaseline;
        const uint32_t clock = 29829;
        std::memcpy(&invalidClock[0], &clock, sizeof(clock));
        APU clockTarget;
        std::istringstream clockInput(invalidClock); clockTarget.loadState(clockInput);
        check(clockInput.fail(), "APU accepted expired frame counter");
        auto invalidSamples = apuBaseline;
        const uint32_t excessiveSamples = 1000;
        const size_t sampleCountOffset = 4 + 1 + 4 + 1 + 1 + sizeof(double) + sizeof(float);
        std::memcpy(&invalidSamples[sampleCountOffset], &excessiveSamples, sizeof(excessiveSamples));
        APU sampleTarget;
        std::istringstream sampleInput(invalidSamples); sampleTarget.loadState(sampleInput);
        check(sampleInput.fail(), "APU accepted excessive sample count");
        auto invalidFilter = apuBaseline;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const size_t filterOffset = invalidFilter.size() - 2 * sizeof(float) - 6 * sizeof(double);
        std::memcpy(&invalidFilter[filterOffset], &nan, sizeof(nan));
        APU filterTarget;
        std::istringstream filterInput(invalidFilter); filterTarget.loadState(filterInput);
        check(filterInput.fail(), "APU accepted non-finite filter state");

        // Legitimate intermediate states must remain loadable even when the
        // host chooses a different audio rate after the snapshot was made.
        APU running, resumed;
        running.setAudioSampleRate(8000.0f);
        resumed.setAudioSampleRate(192000.0f);
        running.cpuWrite(0x4015, 0x0f);
        running.cpuWrite(0x4000, 0xbf);
        running.cpuWrite(0x4001, 0xff);
        running.cpuWrite(0x4002, 40);
        running.cpuWrite(0x4003, 8);
        for (int tick = 0; tick < 80000; ++tick)
        {
            if (tick % 997 == 0) running.cpuWrite(0x4017, (tick / 997) % 2 ? 0x80 : 0);
            running.step();
            if (tick % 97 == 0)
            {
                std::istringstream input(saved(running));
                resumed.loadState(input);
                check(bool(input), "Valid intermediate APU state rejected");
            }
        }

        PPU ppu;
        state = saved(ppu);
        // R2NK PPU layout: CIRAM, registers, decay, timing and background pipeline.
        constexpr size_t spriteCount = 2048 + 2 + 4 + 1 + 4 + 2 + 2 + 1 + 1 + 1 + 32 + 4 + 2 + 2 + 4 + 1 + 4 + 8 + 1;
        static_assert(sizeof(int) == 4, "R2NK fixture requires 32-bit int");
        check(state.size() > spriteCount + sizeof(int), "PPU state too small");
        int invalid = 65;
        std::memcpy(&state[spriteCount], &invalid, sizeof(invalid));
        std::istringstream ppuInput(state); ppu.loadState(ppuInput);
        check(ppuInput.fail(), "PPU accepted out-of-range sprite count");
        PPU cleanPpu;
        state = saved(cleanPpu);
        constexpr size_t fineX = 2048 + 2 + 4 + 1 + 4 + 2 + 2;
        state[fineX] = char(255);
        std::istringstream ppuShift(state); cleanPpu.loadState(ppuShift);
        check(ppuShift.fail(), "PPU accepted out-of-range fine X shift");

        CPU cpu;
        state = saved(cpu);
        constexpr size_t cpuFlag = 4 + 2 + 1 + 1 + 2 + 1 + 2 + 2 + 1 + 1 + 1 + 1 + 2 + 2 + 2;
        check(state.size() > cpuFlag, "CPU state too small");
        state[cpuFlag] = char(2);
        std::istringstream cpuInput(state); cpu.loadState(cpuInput);
        check(cpuInput.fail(), "CPU accepted invalid bool representation");
        CPU cleanCpu;
        state = saved(cleanCpu);
        const uint16_t excessiveCycles = 9;
        std::memcpy(&state[8], &excessiveCycles, sizeof(excessiveCycles));
        std::istringstream cpuCycles(state); cleanCpu.loadState(cpuCycles);
        check(cpuCycles.fail(), "CPU accepted excessive remaining cycles");
        CPU freshCpu;
        state = saved(freshCpu);
        state[15] = char(9);
        std::istringstream cpuMicro(state); freshCpu.loadState(cpuMicro);
        check(cpuMicro.fail(), "CPU accepted excessive microcycle");

        const std::filesystem::path dir(argv[1]); std::filesystem::create_directories(dir);
        std::string rom(16 + 16384, '\0'); rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a; rom[4] = 1;
        const auto path = dir / "state-security.nes";
        std::ofstream romFile(path, std::ios::binary); romFile.write(rom.data(), rom.size()); romFile.close();
        NES nes(""); nes.setCheatDirectory(dir / "no-cheats");
        check(nes.insertCartridge(path.string()), "fixture ROM rejected");
        for (int i = 0; i < 23; ++i) nes.step();
        std::ostringstream before; check(nes.saveState(before), "console save failed");
        const auto ppuState = saved(nes.getPpu());
        auto damaged = before.str();
        const size_t ppuOffset = damaged.find(ppuState);
        check(ppuOffset != std::string::npos && ppuOffset + spriteCount + sizeof(invalid) <= damaged.size(), "PPU payload not found");
        std::memcpy(&damaged[ppuOffset + spriteCount], &invalid, sizeof(invalid));
        std::istringstream corrupt(damaged);
        check(!nes.loadState(corrupt), "Console accepted invalid PPU count");
        std::ostringstream after; check(nes.saveState(after) && after.str() == before.str(), "Console rollback failed");
        const auto cpuState = saved(nes.getCpu());
        const size_t cpuOffset = before.str().find(cpuState);
        check(cpuOffset != std::string::npos, "CPU payload not found");
        damaged = before.str(); damaged[cpuOffset + cpuFlag] = char(2);
        std::istringstream badCpu(damaged);
        check(!nes.loadState(badCpu), "Console accepted invalid CPU bool");
        std::ostringstream afterCpu; check(nes.saveState(afterCpu) && afterCpu.str() == before.str(), "CPU rollback failed");
        const auto apuState = saved(nes.getApu());
        const size_t apuOffset = ppuOffset + ppuState.size();
        check(before.str().compare(apuOffset, apuState.size(), apuState) == 0, "APU payload not found");
        damaged = before.str(); damaged[apuOffset + 32] = char(255); // pulse 1 duty mode
        std::istringstream badApu(damaged);
        check(!nes.loadState(badApu), "Console accepted invalid APU duty mode");
        std::ostringstream afterApu; check(nes.saveState(afterApu) && afterApu.str() == before.str(), "APU rollback failed");
        std::cout << "Invalid save-state values rejected\n";
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
