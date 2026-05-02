#pragma once
#include "Core/NESBoard.h"
#include <string>
#include <fstream>

namespace R2NES::Core
{
    class NESTest
    {
    public:
        NESTest(NesBoard& board);
        ~NESTest();

        bool run(const std::string& romPath, int maxLines = 8991);

    private:
        NesBoard& nes;
        std::ofstream logFile;
        uint32_t totalCycles = 7;

        std::string formatInstruction(uint16_t pc, int& outBytes);
        void writeLog(uint16_t pc, int bytes, const std::string& instStr);
    };
}