#include <iostream>
#include <iomanip>
#include <fstream>
#include "Core/NESBoard.h" // Inclui o cabeçalho da NesBoard

int main()
{
    std::cout << "Starting R2NES v2 Emulator..." << std::endl;

    R2NES::Core::NesBoard nes; // Instancia a NesBoard

    // Caminho para a ROM que você deseja testar
    const std::string romPath = "..\\nestest\\nestest.nes";
    nes.insertCartridge(romPath); // Insere o cartucho

    // O reset() já é chamado no construtor da NesBoard, mas chamá-lo novamente aqui
    // garante que os testes de leitura da ROM no reset sejam executados APÓS o cartucho ser inserido.
    nes.reset();

    // O reset do 6502 leva 8 ciclos. Vamos consumi-los agora para que a primeira
    // instrução em 0xC000 comece imediatamente sem atraso.
    while (!nes.getCpu().complete()) nes.getCpu().clock();

    // Para o nestest rodar de forma automática e testar todas as instruções,
    // devemos forçar o PC para 0xC000.
    nes.getCpu().pc = 0xC000;

    // O nestest em modo automatizado espera que o ciclo inicial seja 7
    // (alguns logs consideram o tempo gasto no reset)
    uint32_t totalCycles = 7; 

    std::ofstream logFile("nestest_R2NES.log");
    if (!logFile.is_open()) {
        std::cerr << "Could not create log file!" << std::endl;
        return -1;
    }

    std::cout << "Running nestest (9000 lines)..." << std::endl;

    // Loop de execução simples para debug via console
    int lineCount = 0;
    while (lineCount < 9000)
    {
        uint16_t currentPC = nes.getCpu().pc;
        uint8_t opcode = nes.cpuRead(currentPC);
        auto& inst = nes.getCpu().lookup[opcode];

        // Determina quantos bytes a instrução tem para imprimir no log
        int bytes = 1;
        if (inst.addrmode == &R2NES::Core::CPU::IMM || inst.addrmode == &R2NES::Core::CPU::ZP0 ||
            inst.addrmode == &R2NES::Core::CPU::ZPX || inst.addrmode == &R2NES::Core::CPU::ZPY ||
            inst.addrmode == &R2NES::Core::CPU::REL) bytes = 2;
        else if (inst.addrmode == &R2NES::Core::CPU::ABS || inst.addrmode == &R2NES::Core::CPU::ABX ||
                 inst.addrmode == &R2NES::Core::CPU::ABY || inst.addrmode == &R2NES::Core::CPU::IND ||
                 inst.addrmode == &R2NES::Core::CPU::IZX || inst.addrmode == &R2NES::Core::CPU::IZY) bytes = 3;

        // 1. PC e Bytes Brutos (Hex)
        logFile << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << currentPC << "  ";
        
        for (int i = 0; i < 3; i++) {
            if (i < bytes) logFile << std::setw(2) << (int)nes.cpuRead(currentPC + i) << " ";
            else logFile << "   ";
        }

        // 2. Nome da Instrução e Espaçamento (Resetamos o preenchimento para espaço aqui)
        logFile << " " << std::setfill(' ') << std::left << std::setw(31) << inst.name;

        // 3. Registradores e Estado
        logFile << std::right << "A:" << std::setfill('0') << std::setw(2) << (int)nes.getCpu().a << " "
                  << "X:" << std::setw(2) << (int)nes.getCpu().x << " "
                  << "Y:" << std::setw(2) << (int)nes.getCpu().y << " "
                  << "P:" << std::setw(2) << (int)nes.getCpu().status << " "
                  << "SP:" << std::setw(2) << (int)nes.getCpu().stkp << " ";
        
        // 4. PPU e Ciclos (CYC usa decimal)
        logFile << std::dec << std::setfill(' ') 
                  << "PPU:" << std::setw(3) << 0 << "," << std::setw(3) << 21 << " " 
                  << "CYC:" << totalCycles << "\n";

        uint32_t cyclesBefore = nes.getSystemClockCounter();
        nes.step(); // Executa uma instrução completa
        totalCycles += (nes.getSystemClockCounter() - cyclesBefore);
        lineCount++;

        // Endereço $0002 armazena o código de erro. 0x00 significa "OK" ou "rodando".
        uint8_t error1 = nes.cpuRead(0x0002);
        uint8_t error2 = nes.cpuRead(0x0003);
        if (error1 != 0x00) {
            std::cout << "\nTEST FAILED! Error code: " << std::hex << (int)error1 
                      << " at PC: " << nes.getCpu().pc << std::endl;
            break;
        }
    }

    logFile.close();
    std::cout << "Done! Log saved to nestest_R2NES.log" << std::endl;
    return 0;
}