#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace R2NES::Core;

static void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

static void cycle(CPU &cpu, int count)
{
    while (count-- > 0)
        cpu.clock();
}

static void restoreCPU(CPU &cpu)
{
    std::ostringstream output;
    cpu.saveState(output);
    std::istringstream input(output.str());
    cpu.loadState(input);
    check(bool(input), "mid-interrupt CPU state rejected");
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "expected temporary directory");
        std::filesystem::create_directories(argv[1]);
        const auto path = std::filesystem::path(argv[1]) / "interrupts.nes";
        std::vector<uint8_t> rom(16 + 0x4000, 0xEA);
        std::fill_n(rom.begin(), 16, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1A;
        rom[4] = 1;
        rom[16] = 0x00; // BRK at $8000.
        rom[16 + 0x3FFA] = 0x00;
        rom[16 + 0x3FFB] = 0x90;
        rom[16 + 0x3FFC] = 0x00;
        rom[16 + 0x3FFD] = 0x80;
        rom[16 + 0x3FFE] = 0x00;
        rom[16 + 0x3FFF] = 0xA0;
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        file.close();

        for (int mode = 0; mode < 2; ++mode)
            for (int edgeAfter = 1; edgeAfter <= 6; ++edgeAfter)
            {
                NES nes("");
                nes.setCheatDirectory(std::filesystem::path(argv[1]) / "no-cheats");
                check(nes.insertCartridge(path.string()), "ROM failed to load");
                auto &cpu = nes.getCpu();
                auto &bus = nes.getBus();
                while (!cpu.complete())
                    cpu.clock();
                cpu.stkp = 0xFD;
                cpu.status = CPU::U;
                if (mode == 1)
                {
                    // A NOP polls the asserted IRQ, then starts its seven cycles.
                    cpu.pc = 0x8002;
                    cpu.setIrqLine(true);
                    cycle(cpu, 2);
                }
                cycle(cpu, edgeAfter);
                cpu.setIrqLine(false);
                cpu.nmi();
                if (edgeAfter < 5)
                {
                    cycle(cpu, 5 - edgeAfter);
                    restoreCPU(cpu); // A escolha do vetor precisa sobreviver ao save.
                    cycle(cpu, 2);
                }
                else
                {
                    restoreCPU(cpu);
                    cycle(cpu, 7 - edgeAfter);
                }
                const bool hijacked = edgeAfter <= 4;
                check(cpu.pc == (hijacked ? 0x9000 : 0xA000), "wrong BRK/IRQ vector for NMI edge");
                check(cpu.stkp == 0xFA, "interrupt did not push three bytes");
                check((bus.ram->read(0x1FB) & CPU::B) == (mode == 0 ? CPU::B : 0), "incorrect stacked B flag");
                const unsigned returnPc = (bus.ram->read(0x1FD) << 8) | bus.ram->read(0x1FC);
                check(returnPc == (mode == 0 ? 0x8002 : 0x8003), "incorrect stacked PC");
                if (!hijacked)
                {
                    cycle(cpu, 2); // One instruction from IRQ/BRK handler first.
                    check(cpu.pc == 0xA001, "late NMI skipped handler instruction");
                    cycle(cpu, 7);
                    check(cpu.pc == 0x9000, "late NMI was lost");
                }
                else
                {
                    cycle(cpu, 2);
                    check(cpu.pc == 0x9001, "hijacking NMI was delivered twice");
                }
            }
        for (uint8_t opcode : {uint8_t{0x58}, uint8_t{0x78}, uint8_t{0x28}, uint8_t{0x40}})
        {
            NES nes("");
            nes.setCheatDirectory(std::filesystem::path(argv[1]) / "no-cheats");
            check(nes.insertCartridge(path.string()), "IRQ latency ROM failed to load");
            auto &cpu = nes.getCpu();
            auto &ram = *nes.getBus().ram;
            while (!cpu.complete())
                cpu.clock();
            cpu.pc = 0x40;
            cpu.x = 0;
            cpu.status = opcode == 0x78 ? CPU::U : CPU::U | CPU::I;
            cpu.stkp = (opcode == 0x28 || opcode == 0x40) ? 0xFA : 0xFD;
            ram.write(0x40, opcode);
            ram.write(0x41, 0xE8);    // INX, executed only when IRQ is delayed.
            ram.write(0x1FB, CPU::U); // PLP / RTI restores I=0.
            ram.write(0x1FC, 0x41);
            ram.write(0x1FD, 0);
            cpu.setIrqLine(true);
            cycle(cpu, opcode == 0x28 ? 4 : opcode == 0x40 ? 6
                                                           : 2);
            if (opcode == 0x58 || opcode == 0x28)
            {
                cycle(cpu, 2);
                check(cpu.x == 1, "CLI/PLP did not delay IRQ for one instruction");
            }
            cpu.setIrqLine(false);
            cycle(cpu, 7);
            check(cpu.pc == 0xA000, "IRQ not accepted at expected boundary");
            if (opcode == 0x78)
                check((ram.read(0x1FB) & CPU::I) != 0, "SEI IRQ did not stack updated I");
            if (opcode == 0x40)
                check(cpu.x == 0, "RTI delayed IRQ after restoring I=0");
        }
        // O formato atual valida o vetor e rejeita estados antigos sem alterar
        // o console, mesmo durante uma instrução de interrupção.
        {
            NES nes("");
            nes.setCheatDirectory(std::filesystem::path(argv[1]) / "no-cheats");
            check(nes.insertCartridge(path.string()), "state fixture ROM failed to load");
            auto &cpu = nes.getCpu();
            while (!cpu.complete())
                cpu.clock();
            cycle(cpu, 5);
            std::ostringstream output;
            check(nes.saveState(output), "state fixture save failed");
            const std::string valid = output.str();
            check(valid.size() > 45, "state fixture too small");
            std::string state = valid;
            state[45] = '\xA5';
            std::istringstream invalid(state);
            check(!nes.loadState(invalid), "R2NL accepted an invalid BRK vector selection");
            state = valid;
            state[0] = '\x4B'; // R2NK on little-endian hosts.
            std::istringstream input(state);
            check(!nes.loadState(input), "legacy R2NK accepted");
            std::ostringstream after;
            check(nes.saveState(after) && after.str() == valid, "rejected state changed console");
            cycle(cpu, 2);
            check(cpu.pc == 0xA000, "rejected state changed BRK vector");
        }
        std::cout << "BRK/IRQ NMI hijack timing and save-state tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}