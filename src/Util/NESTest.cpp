#include "Util/NESTest.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace R2NES::Core
{
    NESTest::NESTest(NesBoard& board) : nes(board)
    {
    }

    NESTest::~NESTest()
    {
        if (logFile.is_open()) logFile.close();
    }

    bool NESTest::run(const std::string& romPath, int maxLines)
    {
        nes.insertCartridge(romPath);
        nes.reset();

        // Consome ciclos de reset
        while (!nes.getCpu().complete()) nes.getCpu().clock();

        // Setup automático do nestest
        nes.getCpu().pc = 0xC000;
        totalCycles = 7; 

        logFile.open("nestest_R2NES.log");
        if (!logFile.is_open()) return false;

        std::cout << "Running NESTest: " << romPath << " (" << maxLines << " lines)..." << std::endl;

        for (int line = 0; line < maxLines; ++line)
        {
            uint16_t currentPC = nes.getCpu().pc;
            int bytes = 1;
            std::string instStr = formatInstruction(currentPC, bytes);
            
            writeLog(currentPC, bytes, instStr);

            uint32_t cyclesBefore = nes.getSystemClockCounter();
            nes.step();
            totalCycles += (nes.getSystemClockCounter() - cyclesBefore);

            // Check de erros do nestest
            uint8_t error = nes.cpuRead(0x0002);
            if (error != 0x00) {
                std::cout << "TEST FAILED! Error code: " << std::hex << (int)error 
                          << " at PC: 0x" << nes.getCpu().pc << std::endl;
                return false;
            }
        }

        std::cout << "NESTest completed successfully. Log saved." << std::endl;
        return true;
    }

    std::string NESTest::formatInstruction(uint16_t pc, int& outBytes)
    {
        uint8_t opcode = nes.cpuRead(pc);
        auto& inst = nes.getCpu().lookup[opcode];
        std::stringstream ss;
        
        outBytes = 1;
        std::string opStr = "";

        if (inst.addrmode == &CPU::IMM) {
            outBytes = 2;
            ss << "#$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)nes.cpuRead(pc + 1);
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ZP0) {
            outBytes = 2;
            uint8_t addr = nes.cpuRead(pc + 1);
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)addr << " = " << std::setw(2) << (int)nes.cpuRead(addr);
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::REL) {
            outBytes = 2;
            int8_t rel = (int8_t)nes.cpuRead(pc + 1);
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << (pc + 2 + rel);
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ABS) {
            outBytes = 3;
            uint16_t addr = (uint16_t)nes.cpuRead(pc + 1) | ((uint16_t)nes.cpuRead(pc + 2) << 8);
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr;
            if (inst.name != "JMP" && inst.name != "JSR") ss << " = " << std::setw(2) << (int)nes.cpuRead(addr);
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ZPX) {
            outBytes = 2;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)nes.cpuRead(pc + 1) << ",X";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ZPY) {
            outBytes = 2;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)nes.cpuRead(pc + 1) << ",Y";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IZX) {
            outBytes = 2;
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)nes.cpuRead(pc + 1) << ",X)";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IZY) {
            outBytes = 2;
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)nes.cpuRead(pc + 1) << "),Y";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ABX) {
            outBytes = 3;
            uint16_t addr = (uint16_t)nes.cpuRead(pc + 1) | ((uint16_t)nes.cpuRead(pc + 2) << 8);
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr << ",X";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ABY) {
            outBytes = 3;
            uint16_t addr = (uint16_t)nes.cpuRead(pc + 1) | ((uint16_t)nes.cpuRead(pc + 2) << 8);
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr << ",Y";
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IND) {
            outBytes = 3;
            uint16_t addr = (uint16_t)nes.cpuRead(pc + 1) | ((uint16_t)nes.cpuRead(pc + 2) << 8);
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << addr << ")";
            opStr = ss.str();
        }

        return inst.name + " " + opStr;
    }

    void NESTest::writeLog(uint16_t pc, int bytes, const std::string& instStr)
    {
        logFile << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << pc << "  ";
        
        for (int i = 0; i < 3; i++) {
            if (i < bytes) logFile << std::setw(2) << (int)nes.cpuRead(pc + i) << " ";
            else logFile << "   ";
        }

        logFile << " " << std::setfill(' ') << std::left << std::setw(32) << instStr;

        logFile << std::right << "A:" << std::setfill('0') << std::setw(2) << (int)nes.getCpu().a << " "
                  << "X:" << std::setw(2) << (int)nes.getCpu().x << " "
                  << "Y:" << std::setw(2) << (int)nes.getCpu().y << " "
                  << "P:" << std::setw(2) << (int)nes.getCpu().status << " "
                  << "SP:" << std::setw(2) << (int)nes.getCpu().stkp << " ";
        
        logFile << std::dec << std::setfill(' ') 
                  << "PPU:" << std::setw(3) << ((totalCycles * 3) / 341) << "," << std::setw(3) << ((totalCycles * 3) % 341) << " " 
                  << "CYC:" << totalCycles << "\n";
    }
}