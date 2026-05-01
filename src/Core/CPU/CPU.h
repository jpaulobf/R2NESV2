#pragma once
#include <cstdint>

namespace R2NES::Core {
    // Forward declaration do Bus para evitar inclusão circular
    class Bus;

    class CPU {
    public:
        CPU();
        ~CPU();

        // Enum para as Flags do Status Register (P)
        // Usamos bitmasks para facilitar a manipulação
        enum FLAGS6502 {
            C = (1 << 0), // Carry Bit
            Z = (1 << 1), // Zero
            I = (1 << 2), // Disable Interrupts
            D = (1 << 3), // Decimal Mode (não usado no NES, mas existe no 6502)
            B = (1 << 4), // Break
            U = (1 << 5), // Unused (Sempre 1)
            V = (1 << 6), // Overflow
            N = (1 << 7), // Negative
        };

        // Conecta a CPU ao barramento principal
        void connectBus(Bus* n);

        // Executa um ciclo de clock da CPU
        void clock();

        // Reseta a CPU para um estado inicial conhecido
        void reset();

        // Métodos auxiliares para manipular as flags
        // Retorna o estado de uma flag específica
        uint8_t GetFlag(FLAGS6502 f);
        
        // Define ou limpa uma flag específica
        void SetFlag(FLAGS6502 f, bool v);

        // Registradores do 6502
        uint8_t a = 0x00; // Acumulador
        uint8_t x = 0x00; // Registrador de Índice X
        uint8_t y = 0x00; // Registrador de Índice Y
        uint8_t stkp = 0x00; // Stack Pointer (Ponteiro de Pilha)
        uint16_t pc = 0x0000; // Program Counter (Contador de Programa)
        uint8_t status = 0x00; // Registrador de Status (Flags)

    private:
        // Variáveis auxiliares para o estado da execução
        uint8_t  fetched     = 0x00;   // Armazena o dado lido para a instrução atual
        uint16_t addr_abs    = 0x0000; // Endereço calculado pela instrução
        uint16_t addr_rel    = 0x0000; // Endereço relativo para saltos (branch)
        uint8_t  opcode      = 0x00;   // Instrução atual
        uint8_t  cycles      = 0;      // Quantos ciclos restam para a instrução terminar

    private:
        Bus* bus = nullptr; // Ponteiro para o Bus ao qual a CPU está conectada
    };
}