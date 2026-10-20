#include "Core/NES.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char **argv)
{
    using namespace R2NES::Core;
    if (argc != 2) return 1;
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    const auto path = (directory / "cpu-cycle-comparison.nes").string();
    std::vector<uint8_t> rom(16 + 0x4000, 0xEA);
    std::fill_n(rom.begin(), 16, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A; rom[4] = 1;
    for (int op = 0; op < 256; ++op) {
        rom[16 + op * 16] = static_cast<uint8_t>(op);
        rom[17 + op * 16] = 0xFF;
        rom[18 + op * 16] = 0x00;
    }
    for (int vector = 0x3FFA; vector < 0x4000; vector += 2) {
        rom[16 + vector] = 0;
        rom[17 + vector] = 0x80;
    }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
    file.close();
    NES nes("");
    nes.setCheatDirectory(directory / "no-cheats");
    nes.insertCartridge(path);
    auto &cpu = nes.getCpu();
    auto &bus = nes.getBus();
    while (!cpu.complete()) cpu.clock();
    std::ostringstream saved;
    if (!nes.saveState(saved)) return 2;
    const auto initial = saved.str();
    uint64_t hash = 14695981039346656037ULL;
    int testedCycles = 0;
    for (int op = 0; op < 256; ++op) {
        for (int scenario = 0; scenario < 8; ++scenario) {
            std::istringstream input(initial);
            if (!nes.loadState(input)) return 3;
            cpu.pc = 0x8000 + op * 16;
            cpu.a = static_cast<uint8_t>(scenario * 37);
            cpu.x = static_cast<uint8_t>(scenario & 1 ? 1 : 0xFF);
            cpu.y = static_cast<uint8_t>(scenario & 2 ? 0 : 0x80);
            cpu.status = static_cast<uint8_t>(scenario & 1 ? 0xFF : 0x20);
            for (int address = 0; address < 0x800; ++address)
                bus.ram->write(address, static_cast<uint8_t>(address * 17 + scenario));
            for (int cycle = 0; cycle < 12; ++cycle) {
                cpu.setIrqLine(scenario >= 4 && cycle >= 2 && cycle <= 5);
                if (scenario >= 6 && cycle == 3) cpu.nmi();
                ++bus.cpuCycleCounter;
                cpu.clock();
                ++bus.systemClockCounter;
                std::ostringstream output;
                cpu.saveState(output);
                bus.ram->saveState(output);
                output.put(static_cast<char>(bus.cpuDataBus));
                const auto bytes = output.str();
                for (unsigned char byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
                // Carregar no meio de uma instrução deve preservar a classificação.
                if (cycle == 2) {
                    std::ostringstream current;
                    if (!nes.saveState(current)) return 4;
                    std::istringstream restore(current.str());
                    if (!nes.loadState(restore)) return 5;
                }
                ++testedCycles;
            }
        }
    }
    // Referência registrada antes de otimizar a classificação de instruções.
    // Compara estado da CPU, RAM e open bus após cada ciclo dos 256 opcodes,
    // variando flags/índices, IRQ/NMI e restauração no meio da instrução.
    constexpr uint64_t referenceDigest = 11572853813347449568ULL;
    if (hash != referenceDigest)
    {
        std::cerr << "CPU cycle regression: expected " << referenceDigest << ", got " << hash << '\n';
        return 6;
    }
    std::cout << "CPU cycle regression passed: 256 opcodes, 8 scenarios, " << testedCycles << " cycles\n";
}
