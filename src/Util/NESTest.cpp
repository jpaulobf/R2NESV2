#include "Util/NESTest.h"
#include <iostream>
#include <iomanip>
#include <set>
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

    // Conjunto de instruções que leem da memória e devem ter o valor logado
    static const std::set<std::string> instructionsThatLogValue = {
        "LDA", "LDX", "LDY", "CMP", "CPX", "CPY", "BIT", "ADC", "SBC", "AND", "ORA", "EOR",
        "ASL", "LSR", "ROL", "ROR", "INC", "DEC", // Instruções RMW (logam o valor antes da modificação)
        "LAX", "ANC", "RLA", "SRE", "RRA", "ISB", "ALR", "ARR", "XAA", "DCP", "SBX", "LAS" // Opcodes ilegais
    };

    // Opcodes não oficiais (ilegais) que o nestest marca com um asterisco '*'
    static const std::set<uint8_t> illegalOpcodes = {
        0x02, 0x03, 0x04, 0x07, 0x0B, 0x0C, 0x0F, 0x12, 0x13, 0x14, 0x17, 0x1A, 0x1B, 0x1C, 0x1F,
        0x22, 0x23, 0x27, 0x2B, 0x2F, 0x32, 0x33, 0x34, 0x37, 0x3A, 0x3B, 0x3C, 0x3F, 0x42, 0x43,
        0x44, 0x47, 0x4B, 0x4F, 0x52, 0x53, 0x54, 0x57, 0x5A, 0x5B, 0x5C, 0x5F, 0x62, 0x63, 0x64,
        0x67, 0x6B, 0x6F, 0x72, 0x73, 0x74, 0x77, 0x7A, 0x7B, 0x7C, 0x7F, 0x80, 0x82, 0x83, 0x87,
        0x89, 0x8B, 0x8F, 0x93, 0x97, 0x9B, 0x9C, 0x9E, 0x9F, 0xA3, 0xA7, 0xAB, 0xAF, 0xB3, 0xB7,
        0xBB, 0xBF, 0xC2, 0xC3, 0xC7, 0xCB, 0xCF, 0xD2, 0xD3, 0xD4, 0xD7, 0xDA, 0xDB, 0xDC, 0xDF,
        0xE2, 0xE3, 0xE7, 0xEB, 0xEF, 0xF2, 0xF3, 0xF4, 0xF7, 0xFA, 0xFB, 0xFC, 0xFF
    };

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
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ZPX) {

            outBytes = 2;
            uint8_t base_addr = nes.cpuRead(pc + 1);
            uint8_t effective_zp_addr = (base_addr + nes.getCpu().x) & 0xFF;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)base_addr << ",X @ " << std::setw(2) << (int)effective_zp_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_zp_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ZPY) {
            outBytes = 2;
            uint8_t base_addr = nes.cpuRead(pc + 1);
            uint8_t effective_zp_addr = (base_addr + nes.getCpu().y) & 0xFF;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)base_addr << ",Y @ " << std::setw(2) << (int)effective_zp_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_zp_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IZX) {
            outBytes = 2;
            uint8_t operand = nes.cpuRead(pc + 1);
            uint8_t zp_indexed_addr = (operand + nes.getCpu().x) & 0xFF;
            uint8_t lo = nes.cpuRead(zp_indexed_addr);
            uint8_t hi = nes.cpuRead((zp_indexed_addr + 1) & 0xFF);
            uint16_t effective_abs_addr = (static_cast<uint16_t>(hi) << 8) | lo;
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)operand << ",X) @ " << std::setw(2) << (int)zp_indexed_addr << " = " << std::setw(4) << effective_abs_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_abs_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IZY) {
            outBytes = 2;
            uint8_t operand = nes.cpuRead(pc + 1);
            uint8_t lo = nes.cpuRead(operand);
            uint8_t hi = nes.cpuRead((operand + 1) & 0xFF);
            uint16_t base_addr = (static_cast<uint16_t>(hi) << 8) | lo;
            uint16_t effective_abs_addr = base_addr + nes.getCpu().y;
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)operand << "),Y = " << std::setw(4) << effective_abs_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_abs_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ABX) {
            outBytes = 3;
            uint8_t lo = nes.cpuRead(pc + 1);
            uint8_t hi = nes.cpuRead(pc + 2);
            uint16_t base_addr = (static_cast<uint16_t>(hi) << 8) | lo;
            uint16_t effective_abs_addr = base_addr + nes.getCpu().x;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << base_addr << ",X @ " << std::setw(4) << effective_abs_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_abs_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::ABY) {
            outBytes = 3;
            uint8_t lo = nes.cpuRead(pc + 1);
            uint8_t hi = nes.cpuRead(pc + 2);
            uint16_t base_addr = (static_cast<uint16_t>(hi) << 8) | lo;
            uint16_t effective_abs_addr = base_addr + nes.getCpu().y;
            ss << "$" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << base_addr << ",Y @ " << std::setw(4) << effective_abs_addr;
            if (instructionsThatLogValue.count(inst.name)) {
                ss << " = " << std::setw(2) << (int)nes.cpuRead(effective_abs_addr);
            }
            opStr = ss.str();
        } else if (inst.addrmode == &CPU::IND) {
            outBytes = 3;
            uint8_t lo_ptr = nes.cpuRead(pc + 1);
            uint8_t hi_ptr = nes.cpuRead(pc + 2);
            uint16_t ptr_addr = (static_cast<uint16_t>(hi_ptr) << 8) | lo_ptr;

            uint16_t effective_abs_addr;
            // Simula o bug do hardware original do 6502 no JMP indireto
            if (lo_ptr == 0xFF) { // Se o byte baixo é 0xFF, o byte alto é lido de (ptr & 0xFF00)
                effective_abs_addr = (static_cast<uint16_t>(nes.cpuRead(ptr_addr & 0xFF00)) << 8) | nes.cpuRead(ptr_addr);
            } else { // Endereçamento indireto normal
                effective_abs_addr = (static_cast<uint16_t>(nes.cpuRead(ptr_addr + 1)) << 8) | nes.cpuRead(ptr_addr);
            }
            ss << "($" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << ptr_addr << ") = " << std::setw(4) << effective_abs_addr;
            opStr = ss.str();
        }

        return (illegalOpcodes.count(opcode) ? "*" : " ") + inst.name + " " + opStr;
    }

    void NESTest::writeLog(uint16_t pc, int bytes, const std::string& instStr)
    {
        logFile << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << pc << "  ";
        
        for (int i = 0; i < 3; i++) {
            if (i < bytes) logFile << std::setw(2) << (int)nes.cpuRead(pc + i) << " ";
            else logFile << "   ";
        }

        // Adiciona um espaço extra para alinhar o início da instrução na coluna 16
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