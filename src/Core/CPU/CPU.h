#pragma once
#include <cstdint>

namespace R2NES::Core {
    // Forward declaration do Bus para evitar inclusão circular
    class Bus;

    class CPU {
    public:
        CPU();
        ~CPU();

        // Conecta a CPU ao barramento principal
        void connectBus(Bus* n);

        // Executa um ciclo de clock da CPU
        void clock();

        // Reseta a CPU para um estado inicial conhecido
        void reset();

        // Registradores do 6502
        uint8_t a = 0x00; // Acumulador
        uint8_t x = 0x00; // Registrador de Índice X
        uint8_t y = 0x00; // Registrador de Índice Y
        uint8_t stkp = 0x00; // Stack Pointer (Ponteiro de Pilha)
        uint16_t pc = 0x0000; // Program Counter (Contador de Programa)
        uint8_t status = 0x00; // Registrador de Status (Flags)

    private:
        Bus* bus = nullptr; // Ponteiro para o Bus ao qual a CPU está conectada
    };
}