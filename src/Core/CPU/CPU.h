#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>

namespace R2NES::Core
{
    // Forward declaration do Bus para evitar inclusão circular
    class Bus;

    class CPU
    {
    public:
        CPU();
        ~CPU();

        // Enum para as Flags do Status Register (P)
        // Usamos bitmasks para facilitar a manipulação
        enum FLAGS6502
        {
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
        void connectBus(Bus *n);

        // Executa um ciclo de clock da CPU
        void clock();

        // Retorna verdadeiro se a instrução atual terminou
        bool complete() const;

        // Reseta a CPU para um estado inicial conhecido
        void reset();

        // Sinais de interrupção externos
        void irq(); // Interrupt Request
        void nmi(); // Non-Maskable Interrupt

        // Métodos auxiliares para manipular as flags
        // Retorna o estado de uma flag específica
        uint8_t GetFlag(FLAGS6502 f);

        // Define ou limpa uma flag específica
        void SetFlag(FLAGS6502 f, bool v);

        // Retorna um mapa de strings representando a desmontagem do código (Disassembler)
        std::map<uint16_t, std::string> disassemble(uint16_t nStart, uint16_t nStop);

        void push(uint8_t data);
        uint8_t pop();

        // Atualiza as flags Negative e Zero com base em um valor
        void updateNZFlags(uint8_t value);

        // Tabela de instruções do 6502 (256 opcodes)
        // Cada entrada contém o nome da instrução, ponteiros para as funções
        // de operação e modo de endereçamento, e o número base de ciclos
        uint8_t fetch();

        // Estrutura para representar uma instrução do 6502
        struct INSTRUCTION
        {
            std::string name;
            uint8_t (CPU::*operate)(void) = nullptr;  // Função da Operação
            uint8_t (CPU::*addrmode)(void) = nullptr; // Função do Modo de Endereçamento
            uint8_t cycles = 0;                       // Ciclos base
        };

        std::vector<INSTRUCTION> lookup; // Tabela de 256 opcodes

        // Registradores do 6502
        uint8_t a = 0x00;      // Acumulador
        uint8_t x = 0x00;      // Registrador de Índice X
        uint8_t y = 0x00;      // Registrador de Índice Y
        uint8_t stkp = 0x00;   // Stack Pointer (Ponteiro de Pilha)
        uint16_t pc = 0x0000;  // Program Counter (Contador de Programa)
        uint8_t status = 0x00; // Registrador de Status (Flags)

        // --- Modos de Endereçamento (Addressing Modes) ---
        // Retornam 1 se um ciclo extra for necessário (ex: cruzamento de página)
        uint8_t IMP();
        uint8_t IMM();
        uint8_t ZP0();
        uint8_t ZPX();
        uint8_t ZPY();
        uint8_t REL();
        uint8_t ABS();
        uint8_t ABX();
        uint8_t ABY();
        uint8_t IND();
        uint8_t IZX();
        uint8_t IZY();

        // --- Opcodes (Instruções) ---
        // Retornam 1 se um ciclo extra for necessário
        uint8_t ADC();
        uint8_t AND();
        uint8_t ASL();
        uint8_t BCC();
        uint8_t BCS();
        uint8_t BEQ();
        uint8_t BIT();
        uint8_t BMI();
        uint8_t BNE();
        uint8_t BPL();
        uint8_t BRK();
        uint8_t BVC();
        uint8_t BVS();
        uint8_t CLC();
        uint8_t CLD();
        uint8_t CLI();
        uint8_t CLV();
        uint8_t CMP();
        uint8_t CPX();
        uint8_t CPY();
        uint8_t DEC();
        uint8_t DEX();
        uint8_t DEY();
        uint8_t EOR();
        uint8_t INC();
        uint8_t INX();
        uint8_t INY();
        uint8_t JMP();
        uint8_t JSR();
        uint8_t LDA();
        uint8_t LDX();
        uint8_t LDY();
        uint8_t LSR();
        uint8_t NOP();
        uint8_t ORA();
        uint8_t PHA();
        uint8_t PHP();
        uint8_t PLA();
        uint8_t PLP();
        uint8_t ROL();
        uint8_t ROR();
        uint8_t RTI();
        uint8_t RTS();
        uint8_t SBC();
        uint8_t SEC();
        uint8_t SED();
        uint8_t SEI();
        uint8_t STA();
        uint8_t SLO();
        uint8_t STX();
        uint8_t STY();
        uint8_t TAX();
        uint8_t TAY();
        uint8_t TSX();
        uint8_t TXA();
        uint8_t TXS();
        uint8_t TYA();

        uint8_t LAX();
        uint8_t SAX();
        uint8_t DCP();
        uint8_t STP();
        uint8_t ANC();
        uint8_t RLA();
        uint8_t SRE();
        uint8_t RRA();
        uint8_t ISC();
        uint8_t ALR();
        uint8_t ARR();
        uint8_t XAA();

        // Instrução para lidar com Opcodes ilegais
        uint8_t XXX();

    private:
        // Variáveis auxiliares para o estado da execução
        uint8_t fetched = 0x00;     // Armazena o dado lido para a instrução atual
        uint16_t addr_abs = 0x0000; // Endereço calculado pela instrução
        uint16_t addr_rel = 0x0000; // Endereço relativo para saltos (branch)
        uint8_t opcode = 0x00;      // Instrução atual
        uint8_t cycles = 0;         // Quantos ciclos restam para a instrução terminar

    private:
        Bus *bus = nullptr; // Ponteiro para o Bus ao qual a CPU está conectada
    };
}