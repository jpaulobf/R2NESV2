#include "Core/CPU/CPU.h"
#include "Core/Bus/Bus.h"

namespace R2NES::Core
{

    CPU::CPU()
    {
        // Construtor: Inicializa os registradores com valores padrão (geralmente 0)
        // O reset() fará a inicialização específica do 6502
    }

    CPU::~CPU()
    {
    }

    void CPU::connectBus(Bus *bus)
    {
        this->bus = bus;
    }

    // Retorna o estado de uma flag específica
    uint8_t CPU::GetFlag(FLAGS6502 flag)
    {
        return ((status & flag) > 0) ? 1 : 0;
    }

    // Define ou limpa uma flag específica
    void CPU::SetFlag(FLAGS6502 flag, bool set)
    {
        if (set)
        {
            status |= flag; // Define o bit da flag
        }
        else
        {
            status &= ~flag; // Limpa o bit da flag
        }
    }

    void CPU::reset()
    {
        // Valores de reset do 6502
        a = 0x00;
        x = 0x00;
        y = 0x00;
        stkp = 0xFD;   // Stack Pointer inicializa em 0xFD
        status = 0x24; // U e I flags setadas (00100100)

        // Lê o vetor de Reset (0xFFFC e 0xFFFD) para saber por onde começar
        uint16_t lo = bus->cpuRead(0xFFFC);
        uint16_t hi = bus->cpuRead(0xFFFD);
        pc = (hi << 8) | lo;

        // Reset leva 8 ciclos de clock no hardware real
        cycles = 8;
    }

    void CPU::clock()
    {
        // O NES funciona com pulsos de clock.
        // Se 'cycles' for 0, significa que a instrução anterior terminou
        // e podemos buscar a próxima.
        if (cycles == 0)
        {
            // 1. Fetch: Lê o opcode no endereço apontado pelo PC
            opcode = bus->cpuRead(pc);
            pc++;

            // 2. Decode & Execute:
            // Por enquanto vamos apenas simular que toda instrução leva 2 ciclos
            // (Isso será substituído por uma tabela de busca/switch-case em breve)
            cycles = 2;
        }

        // Decrementa o contador de ciclos
        cycles--;
    }
}