#include "Core/CPU/CPU.h"
#include "Core/Bus/Bus.h"
#include <iomanip>
#include <sstream>

namespace R2NES::Core
{
    // Construtor: inicializa a tabela de instruções (lookup)
    // Cada entrada contém: nome, ponteiro para operação, modo de endereçamento e ciclos base.
    CPU::CPU()
    {
        // Inicializa a tabela de opcodes (256 entradas)
        // Formato: { "NOME", Operação, Modo de Endereçamento, Ciclos base }
        lookup = {
            {"BRK", &CPU::BRK, &CPU::IMM, 7},
            {"ORA", &CPU::ORA, &CPU::IZX, 6},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"SLO", &CPU::SLO, &CPU::IZX, 8},
            {"NOP", &CPU::NOP, &CPU::ZP0, 3},
            {"ORA", &CPU::ORA, &CPU::ZP0, 3},
            {"ASL", &CPU::ASL, &CPU::ZP0, 5},
            {"SLO", &CPU::SLO, &CPU::ZP0, 5},
            {"PHP", &CPU::PHP, &CPU::IMP, 3},
            {"ORA", &CPU::ORA, &CPU::IMM, 2},
            {"ASL", &CPU::ASL, &CPU::IMP, 2},
            {"ANC", &CPU::ANC, &CPU::IMM, 2},
            {"NOP", &CPU::NOP, &CPU::ABS, 4},
            {"ORA", &CPU::ORA, &CPU::ABS, 4},
            {"ASL", &CPU::ASL, &CPU::ABS, 6},
            {"SLO", &CPU::SLO, &CPU::ABS, 6},
            {"BPL", &CPU::BPL, &CPU::REL, 2},
            {"ORA", &CPU::ORA, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"SLO", &CPU::SLO, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"ORA", &CPU::ORA, &CPU::ZPX, 4},
            {"ASL", &CPU::ASL, &CPU::ZPX, 6},
            {"SLO", &CPU::SLO, &CPU::ZPX, 6},
            {"CLC", &CPU::CLC, &CPU::IMP, 2},
            {"ORA", &CPU::ORA, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"SLO", &CPU::SLO, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"ORA", &CPU::ORA, &CPU::ABX, 4},
            {"ASL", &CPU::ASL, &CPU::ABX, 7},
            {"SLO", &CPU::SLO, &CPU::ABX, 7},
            {"JSR", &CPU::JSR, &CPU::ABS, 6},
            {"AND", &CPU::AND, &CPU::IZX, 6},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"RLA", &CPU::RLA, &CPU::IZX, 8},
            {"BIT", &CPU::BIT, &CPU::ZP0, 3},
            {"AND", &CPU::AND, &CPU::ZP0, 3},
            {"ROL", &CPU::ROL, &CPU::ZP0, 5},
            {"RLA", &CPU::RLA, &CPU::ZP0, 5},
            {"PLP", &CPU::PLP, &CPU::IMP, 4},
            {"AND", &CPU::AND, &CPU::IMM, 2},
            {"ROL", &CPU::ROL, &CPU::IMP, 2},
            {"ANC", &CPU::ANC, &CPU::IMM, 2},
            {"BIT", &CPU::BIT, &CPU::ABS, 4},
            {"AND", &CPU::AND, &CPU::ABS, 4},
            {"ROL", &CPU::ROL, &CPU::ABS, 6},
            {"RLA", &CPU::RLA, &CPU::ABS, 6},
            {"BMI", &CPU::BMI, &CPU::REL, 2},
            {"AND", &CPU::AND, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"RLA", &CPU::RLA, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"AND", &CPU::AND, &CPU::ZPX, 4},
            {"ROL", &CPU::ROL, &CPU::ZPX, 6},
            {"RLA", &CPU::RLA, &CPU::ZPX, 6},
            {"SEC", &CPU::SEC, &CPU::IMP, 2},
            {"AND", &CPU::AND, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"RLA", &CPU::RLA, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"AND", &CPU::AND, &CPU::ABX, 4},
            {"ROL", &CPU::ROL, &CPU::ABX, 7},
            {"RLA", &CPU::RLA, &CPU::ABX, 7},
            {"RTI", &CPU::RTI, &CPU::IMP, 6},
            {"EOR", &CPU::EOR, &CPU::IZX, 6},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"SRE", &CPU::SRE, &CPU::IZX, 8},
            {"NOP", &CPU::NOP, &CPU::ZP0, 3},
            {"EOR", &CPU::EOR, &CPU::ZP0, 3},
            {"LSR", &CPU::LSR, &CPU::ZP0, 5},
            {"SRE", &CPU::SRE, &CPU::ZP0, 5},
            {"PHA", &CPU::PHA, &CPU::IMP, 3},
            {"EOR", &CPU::EOR, &CPU::IMM, 2},
            {"LSR", &CPU::LSR, &CPU::IMP, 2},
            {"ALR", &CPU::ALR, &CPU::IMM, 2},
            {"JMP", &CPU::JMP, &CPU::ABS, 3},
            {"EOR", &CPU::EOR, &CPU::ABS, 4},
            {"LSR", &CPU::LSR, &CPU::ABS, 6},
            {"SRE", &CPU::SRE, &CPU::ABS, 6},
            {"BVC", &CPU::BVC, &CPU::REL, 2},
            {"EOR", &CPU::EOR, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"SRE", &CPU::SRE, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"EOR", &CPU::EOR, &CPU::ZPX, 4},
            {"LSR", &CPU::LSR, &CPU::ZPX, 6},
            {"SRE", &CPU::SRE, &CPU::ZPX, 6},
            {"CLI", &CPU::CLI, &CPU::IMP, 2},
            {"EOR", &CPU::EOR, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"SRE", &CPU::SRE, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"EOR", &CPU::EOR, &CPU::ABX, 4},
            {"LSR", &CPU::LSR, &CPU::ABX, 7},
            {"SRE", &CPU::SRE, &CPU::ABX, 7},
            {"RTS", &CPU::RTS, &CPU::IMP, 6},
            {"ADC", &CPU::ADC, &CPU::IZX, 6},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"RRA", &CPU::RRA, &CPU::IZX, 8},
            {"NOP", &CPU::NOP, &CPU::ZP0, 3},
            {"ADC", &CPU::ADC, &CPU::ZP0, 3},
            {"ROR", &CPU::ROR, &CPU::ZP0, 5},
            {"RRA", &CPU::RRA, &CPU::ZP0, 5},
            {"PLA", &CPU::PLA, &CPU::IMP, 4},
            {"ADC", &CPU::ADC, &CPU::IMM, 2},
            {"ROR", &CPU::ROR, &CPU::IMP, 2},
            {"ARR", &CPU::ARR, &CPU::IMM, 2},
            {"JMP", &CPU::JMP, &CPU::IND, 5},
            {"ADC", &CPU::ADC, &CPU::ABS, 4},
            {"ROR", &CPU::ROR, &CPU::ABS, 6},
            {"RRA", &CPU::RRA, &CPU::ABS, 6},
            {"BVS", &CPU::BVS, &CPU::REL, 2},
            {"ADC", &CPU::ADC, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"RRA", &CPU::RRA, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"ADC", &CPU::ADC, &CPU::ZPX, 4},
            {"ROR", &CPU::ROR, &CPU::ZPX, 6},
            {"RRA", &CPU::RRA, &CPU::ZPX, 6},
            {"SEI", &CPU::SEI, &CPU::IMP, 2},
            {"ADC", &CPU::ADC, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"RRA", &CPU::RRA, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"ADC", &CPU::ADC, &CPU::ABX, 4},
            {"ROR", &CPU::ROR, &CPU::ABX, 7},
            {"RRA", &CPU::RRA, &CPU::ABX, 7},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"STA", &CPU::STA, &CPU::IZX, 6},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"SAX", &CPU::SAX, &CPU::IZX, 6},
            {"STY", &CPU::STY, &CPU::ZP0, 3},
            {"STA", &CPU::STA, &CPU::ZP0, 3},
            {"STX", &CPU::STX, &CPU::ZP0, 3},
            {"SAX", &CPU::SAX, &CPU::ZP0, 3},
            {"DEY", &CPU::DEY, &CPU::IMP, 2},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"TXA", &CPU::TXA, &CPU::IMP, 2},
            {"XAA", &CPU::XAA, &CPU::IMM, 2},
            {"STY", &CPU::STY, &CPU::ABS, 4},
            {"STA", &CPU::STA, &CPU::ABS, 4},
            {"STX", &CPU::STX, &CPU::ABS, 4},
            {"SAX", &CPU::SAX, &CPU::ABS, 4},
            {"BCC", &CPU::BCC, &CPU::REL, 2},
            {"STA", &CPU::STA, &CPU::IZY, 6},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"SHA", &CPU::SHA, &CPU::IZY, 6},
            {"STY", &CPU::STY, &CPU::ZPX, 4},
            {"STA", &CPU::STA, &CPU::ZPX, 4},
            {"STX", &CPU::STX, &CPU::ZPY, 4},
            {"SAX", &CPU::SAX, &CPU::ZPY, 4},
            {"TYA", &CPU::TYA, &CPU::IMP, 2},
            {"STA", &CPU::STA, &CPU::ABY, 5},
            {"TXS", &CPU::TXS, &CPU::IMP, 2},
            {"TAS", &CPU::TAS, &CPU::ABY, 5},
            {"SHY", &CPU::SHY, &CPU::ABX, 5},
            {"STA", &CPU::STA, &CPU::ABX, 5},
            {"SHX", &CPU::SHX, &CPU::ABY, 5},
            {"SHA", &CPU::SHA, &CPU::ABY, 5},
            {"LDY", &CPU::LDY, &CPU::IMM, 2},
            {"LDA", &CPU::LDA, &CPU::IZX, 6},
            {"LDX", &CPU::LDX, &CPU::IMM, 2},
            {"LAX", &CPU::LAX, &CPU::IZX, 6},
            {"LDY", &CPU::LDY, &CPU::ZP0, 3},
            {"LDA", &CPU::LDA, &CPU::ZP0, 3},
            {"LDX", &CPU::LDX, &CPU::ZP0, 3},
            {"LAX", &CPU::LAX, &CPU::ZP0, 3},
            {"TAY", &CPU::TAY, &CPU::IMP, 2},
            {"LDA", &CPU::LDA, &CPU::IMM, 2},
            {"TAX", &CPU::TAX, &CPU::IMP, 2},
            {"LAX", &CPU::LAX, &CPU::IMM, 2},
            {"LDY", &CPU::LDY, &CPU::ABS, 4},
            {"LDA", &CPU::LDA, &CPU::ABS, 4},
            {"LDX", &CPU::LDX, &CPU::ABS, 4},
            {"LAX", &CPU::LAX, &CPU::ABS, 4},
            {"BCS", &CPU::BCS, &CPU::REL, 2},
            {"LDA", &CPU::LDA, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"LAX", &CPU::LAX, &CPU::IZY, 5},
            {"LDY", &CPU::LDY, &CPU::ZPX, 4},
            {"LDA", &CPU::LDA, &CPU::ZPX, 4},
            {"LDX", &CPU::LDX, &CPU::ZPY, 4},
            {"LAX", &CPU::LAX, &CPU::ZPY, 4},
            {"CLV", &CPU::CLV, &CPU::IMP, 2},
            {"LDA", &CPU::LDA, &CPU::ABY, 4},
            {"TSX", &CPU::TSX, &CPU::IMP, 2},
            {"LAS", &CPU::LAS, &CPU::ABY, 4},
            {"LDY", &CPU::LDY, &CPU::ABX, 4},
            {"LDA", &CPU::LDA, &CPU::ABX, 4},
            {"LDX", &CPU::LDX, &CPU::ABY, 4},
            {"LAX", &CPU::LAX, &CPU::ABY, 4},
            {"CPY", &CPU::CPY, &CPU::IMM, 2}, // C0
            {"CMP", &CPU::CMP, &CPU::IZX, 6},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"DCP", &CPU::DCP, &CPU::IZX, 8},
            {"CPY", &CPU::CPY, &CPU::ZP0, 3},
            {"CMP", &CPU::CMP, &CPU::ZP0, 3},
            {"DEC", &CPU::DEC, &CPU::ZP0, 5},
            {"DCP", &CPU::DCP, &CPU::ZP0, 5},
            {"INY", &CPU::INY, &CPU::IMP, 2},
            {"CMP", &CPU::CMP, &CPU::IMM, 2}, // C9
            {"DEX", &CPU::DEX, &CPU::IMP, 2},
            {"SBX", &CPU::SBX, &CPU::IMM, 2}, // CB
            {"CPY", &CPU::CPY, &CPU::ABS, 4},
            {"CMP", &CPU::CMP, &CPU::ABS, 4},
            {"DEC", &CPU::DEC, &CPU::ABS, 6},
            {"DCP", &CPU::DCP, &CPU::ABS, 6},
            {"BNE", &CPU::BNE, &CPU::REL, 2},
            {"CMP", &CPU::CMP, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"DCP", &CPU::DCP, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"CMP", &CPU::CMP, &CPU::ZPX, 4},
            {"DEC", &CPU::DEC, &CPU::ZPX, 6},
            {"DCP", &CPU::DCP, &CPU::ZPX, 6},
            {"CLD", &CPU::CLD, &CPU::IMP, 2},
            {"CMP", &CPU::CMP, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"DCP", &CPU::DCP, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"CMP", &CPU::CMP, &CPU::ABX, 4},
            {"DEC", &CPU::DEC, &CPU::ABX, 7},
            {"DCP", &CPU::DCP, &CPU::ABX, 7},
            {"CPX", &CPU::CPX, &CPU::IMM, 2},
            {"SBC", &CPU::SBC, &CPU::IZX, 6},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"ISB", &CPU::ISC, &CPU::IZX, 8},
            {"CPX", &CPU::CPX, &CPU::ZP0, 3},
            {"SBC", &CPU::SBC, &CPU::ZP0, 3},
            {"INC", &CPU::INC, &CPU::ZP0, 5},
            {"ISB", &CPU::ISC, &CPU::ZP0, 5},
            {"INX", &CPU::INX, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"CPX", &CPU::CPX, &CPU::ABS, 4},
            {"SBC", &CPU::SBC, &CPU::ABS, 4},
            {"INC", &CPU::INC, &CPU::ABS, 6},
            {"ISB", &CPU::ISC, &CPU::ABS, 6},
            {"BEQ", &CPU::BEQ, &CPU::REL, 2},
            {"SBC", &CPU::SBC, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"ISB", &CPU::ISC, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"SBC", &CPU::SBC, &CPU::ZPX, 4},
            {"INC", &CPU::INC, &CPU::ZPX, 6},
            {"ISB", &CPU::ISC, &CPU::ZPX, 6},
            {"SED", &CPU::SED, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"ISB", &CPU::ISC, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"SBC", &CPU::SBC, &CPU::ABX, 4},
            {"INC", &CPU::INC, &CPU::ABX, 7},
            {"ISB", &CPU::ISC, &CPU::ABX, 7},
        };
    }

    CPU::~CPU()
    {
    }

    void CPU::connectBus(Bus *bus)
    {
        // Guarda o ponteiro para o barramento principal usado em leituras/escritas
        this->bus = bus;
    }

    // Retorna o estado de uma flag específica
    uint8_t CPU::GetFlag(FLAGS6502 flag)
    {
        // Retorna 1 se o bit correspondente estiver setado, 0 caso contrário.
        return ((status & flag) > 0) ? 1 : 0;
    }

    // Define ou limpa uma flag específica
    void CPU::SetFlag(FLAGS6502 flag, bool set)
    {
        // Opera diretamente sobre o registrador `status` (P)
        if (set)
        {
            status |= flag; // Define o bit da flag
        }
        else
        {
            status &= ~flag; // Limpa o bit da flag
        }
    }

    void CPU::updateNZFlags(uint8_t value)
    {
        // Atualiza Zero e Negative com base em `value`.
        SetFlag(Z, value == 0x00);
        SetFlag(N, value & 0x80);
    }

    void CPU::push(uint8_t data)
    {
        // Escrita na pilha (RAM mapeada em 0x0100-0x01FF). Depois decrementa SP.
        bus->cpuWrite(0x0100 + stkp, data);
        stkp--;
    }

    uint8_t CPU::pop()
    {
        // Incrementa SP e lê o valor da pilha.
        stkp++;
        return bus->cpuRead(0x0100 + stkp);
    }

    void CPU::irq()
    {
        if (GetFlag(I) == 0 && interruptKind == 0)
            interruptRequested = true;
    }

    void CPU::nmi()
    {
        pendingNmi = true;
        interruptRequested = true;
    }

    void CPU::reset()
    {
        // Valores de reset do 6502
        a = 0x00;
        x = 0x00;
        y = 0x00;
        status = 0x24; // U e I flags setadas (00100100)

        pc = 0x0000;
        opcode = 0x00;
        fetched = 0x00;
        addr_abs = 0x0000;
        addr_rel = 0x0000;
        cycles = 7;
        microCycle = 0;
        operandLow = 0;
        operandHigh = 0;
        zeroPageAddress = 0;
        baseAddress = 0;
        wrongPageAddress = 0;
        branchTarget = 0;
        interruptKind = 3;
        interruptRequested = false;
        pendingNmi = false;
        jammed = false;
        fetchedCached = false;
        pageCrossed = false;
        branchTaken = false;
        // No reset, o 2A03 executa dois ciclos de leitura do PC, três leituras
        // da pilha e então lê o vetor $FFFC/$FFFD, decrementando SP até $FD.
        stkp = 0x00;
    }

    uint16_t CPU::clock()
    {
        if (cycles == 0)
        {
            if (interruptKind == 0 && interruptRequested)
            {
                interruptKind = pendingNmi ? 2 : 1;
                pendingNmi = false;
                interruptRequested = false;
            }

            if (interruptKind != 0)
            {
                microCycle = 1;
                cycles = 7;
                if (interruptKind == 3)
                    stkp = 0x00;
                bus->cpuRead(pc); // ciclo 1: leitura descartada do PC
                finishCycle();
                return 1;
            }

            if (jammed)
            {
                bus->cpuRead(pc);
                return 1;
            }

            SetFlag(U, true);
            opcode = bus->cpuRead(pc++);
            microCycle = 1;
            cycles = lookup[opcode].cycles;
            finishCycle();
            return 1;
        }

        if (interruptKind != 0)
        {
            ++microCycle;
            executeInstructionCycle();
            finishCycle();
            return 1;
        }

        ++microCycle;
        executeInstructionCycle();
        finishCycle();
        return 1;
    }

    void CPU::finishCycle()
    {
        if (cycles > 0)
            --cycles;
        if (cycles == 0)
        {
            microCycle = 0;
            interruptKind = 0;
            fetchedCached = false;
            SetFlag(U, true);
        }
    }

    bool CPU::isStoreInstruction() const
    {
        auto op = lookup[opcode].operate;
        return op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
               op == &CPU::SAX || op == &CPU::SHA || op == &CPU::SHX ||
               op == &CPU::SHY || op == &CPU::TAS;
    }

    bool CPU::isRmwInstruction() const
    {
        auto op = lookup[opcode].operate;
        return op == &CPU::ASL || op == &CPU::LSR || op == &CPU::ROL || op == &CPU::ROR ||
               op == &CPU::INC || op == &CPU::DEC || op == &CPU::SLO || op == &CPU::RLA ||
               op == &CPU::SRE || op == &CPU::RRA || op == &CPU::DCP || op == &CPU::ISC;
    }

    bool CPU::isBranchInstruction() const
    {
        auto op = lookup[opcode].operate;
        return op == &CPU::BCC || op == &CPU::BCS || op == &CPU::BEQ || op == &CPU::BMI ||
               op == &CPU::BNE || op == &CPU::BPL || op == &CPU::BVC || op == &CPU::BVS;
    }

    void CPU::executeReadOperation()
    {
        fetched = bus->cpuRead(addr_abs);
        fetchedCached = true;
        (this->*lookup[opcode].operate)();
        fetchedCached = false;
    }

    void CPU::executeInstructionCycle()
    {
        const uint8_t c = microCycle;
        const auto mode = lookup[opcode].addrmode;
        const auto op = lookup[opcode].operate;
        const bool store = isStoreInstruction();
        const bool rmw = isRmwInstruction();

        if (interruptKind != 0)
        {
            if (interruptKind == 3)
            {
                if (c <= 2)
                    bus->cpuRead(pc);
                else if (c <= 5)
                {
                    bus->cpuRead(0x0100 | stkp);
                    --stkp;
                }
                else if (c == 6)
                    operandLow = bus->cpuRead(0xFFFC);
                else if (c == 7)
                    pc = (static_cast<uint16_t>(bus->cpuRead(0xFFFD)) << 8) | operandLow;
                return;
            }

            if (c == 2)
                bus->cpuRead(pc);
            else if (c == 3)
                push(static_cast<uint8_t>(pc >> 8));
            else if (c == 4)
                push(static_cast<uint8_t>(pc));
            else if (c == 5)
            {
                uint8_t stackedStatus = static_cast<uint8_t>((status & ~B) | U);
                push(stackedStatus);
                SetFlag(I, true);
            }
            else if (c == 6)
                operandLow = bus->cpuRead(interruptKind == 2 ? 0xFFFA : 0xFFFE);
            else if (c == 7)
            {
                uint16_t vector = interruptKind == 2 ? 0xFFFB : 0xFFFF;
                pc = (static_cast<uint16_t>(bus->cpuRead(vector)) << 8) | operandLow;
            }
            return;
        }

        if (isBranchInstruction())
        {
            if (c == 2)
            {
                addr_rel = static_cast<uint16_t>(static_cast<int8_t>(bus->cpuRead(pc++)));
                const uint16_t oldPC = pc;
                if (op == &CPU::BCC)
                    branchTaken = GetFlag(C) == 0;
                else if (op == &CPU::BCS)
                    branchTaken = GetFlag(C) != 0;
                else if (op == &CPU::BEQ)
                    branchTaken = GetFlag(Z) != 0;
                else if (op == &CPU::BMI)
                    branchTaken = GetFlag(N) != 0;
                else if (op == &CPU::BNE)
                    branchTaken = GetFlag(Z) == 0;
                else if (op == &CPU::BPL)
                    branchTaken = GetFlag(N) == 0;
                else if (op == &CPU::BVC)
                    branchTaken = GetFlag(V) == 0;
                else
                    branchTaken = GetFlag(V) != 0;
                branchTarget = branchTaken ? static_cast<uint16_t>(oldPC + addr_rel) : oldPC;
                pageCrossed = ((oldPC & 0xFF00) != (branchTarget & 0xFF00));
                addr_abs = branchTarget;
                if (branchTaken)
                {
                    ++cycles;
                    if (pageCrossed)
                        ++cycles;
                }
            }
            else if (c == 3)
            {
                bus->cpuRead(pc);
                if (branchTaken)
                {
                    if (pageCrossed)
                        pc = (pc & 0xFF00) | (branchTarget & 0x00FF);
                    else
                        pc = branchTarget;
                }
            }
            else if (c == 4)
            {
                bus->cpuRead(pc);
                pc = branchTarget;
            }
            return;
        }

        if (op == &CPU::BRK)
        {
            if (c == 2)
            {
                bus->cpuRead(pc++);
            }
            else if (c == 3)
                push(static_cast<uint8_t>(pc >> 8));
            else if (c == 4)
                push(static_cast<uint8_t>(pc));
            else if (c == 5)
            {
                push(status | B | U);
                SetFlag(I, true);
            }
            else if (c == 6)
                operandLow = bus->cpuRead(0xFFFE);
            else if (c == 7)
                pc = (static_cast<uint16_t>(bus->cpuRead(0xFFFF)) << 8) | operandLow;
            return;
        }

        if (op == &CPU::JMP)
        {
            if (mode == &CPU::ABS)
            {
                if (c == 2)
                {
                    operandLow = bus->cpuRead(pc++);
                }
                else if (c == 3)
                    pc = (static_cast<uint16_t>(bus->cpuRead(pc)) << 8) | operandLow;
            }
            else // JMP indirect, including the NMOS 6502 page-wrap bug
            {
                if (c == 2)
                    operandLow = bus->cpuRead(pc++);
                else if (c == 3)
                {
                    operandHigh = bus->cpuRead(pc++);
                    baseAddress = (static_cast<uint16_t>(operandHigh) << 8) | operandLow;
                }
                else if (c == 4)
                    operandLow = bus->cpuRead(baseAddress);
                else if (c == 5)
                {
                    uint16_t highAddress = (baseAddress & 0x00FF) == 0x00FF
                                               ? (baseAddress & 0xFF00)
                                               : static_cast<uint16_t>(baseAddress + 1);
                    pc = (static_cast<uint16_t>(bus->cpuRead(highAddress)) << 8) | operandLow;
                }
            }
            return;
        }

        if (op == &CPU::JSR)
        {
            if (c == 2)
                operandLow = bus->cpuRead(pc++);
            else if (c == 3)
                bus->cpuRead(0x0100 | stkp);
            else if (c == 4)
                push(static_cast<uint8_t>(pc >> 8));
            else if (c == 5)
                push(static_cast<uint8_t>(pc));
            else if (c == 6)
                pc = (static_cast<uint16_t>(bus->cpuRead(pc)) << 8) | operandLow;
            return;
        }

        if (op == &CPU::RTS || op == &CPU::RTI)
        {
            if (c == 2)
                bus->cpuRead(pc);
            else if (c == 3)
                bus->cpuRead(0x0100 | stkp);
            else if (op == &CPU::RTS)
            {
                if (c == 4)
                    operandLow = pop();
                else if (c == 5)
                    operandHigh = pop();
                else if (c == 6)
                {
                    uint16_t returnAddress = (static_cast<uint16_t>(operandHigh) << 8) | operandLow;
                    bus->cpuRead(returnAddress);
                    pc = returnAddress + 1;
                }
            }
            else
            {
                if (c == 4)
                {
                    status = pop();
                    SetFlag(B, false);
                    SetFlag(U, true);
                }
                else if (c == 5)
                    operandLow = pop();
                else if (c == 6)
                    pc = (static_cast<uint16_t>(pop()) << 8) | operandLow;
            }
            return;
        }

        if (op == &CPU::PHA || op == &CPU::PHP || op == &CPU::PLA || op == &CPU::PLP)
        {
            if (c == 2)
                bus->cpuRead(pc);
            else if ((op == &CPU::PHA || op == &CPU::PHP) && c == 3)
                (this->*op)();
            else if ((op == &CPU::PLA || op == &CPU::PLP) && c == 3)
                bus->cpuRead(0x0100 | stkp);
            else if (op == &CPU::PLA && c == 4)
            {
                a = pop();
                updateNZFlags(a);
            }
            else if (op == &CPU::PLP && c == 4)
            {
                status = pop();
                SetFlag(B, false);
                SetFlag(U, true);
            }
            return;
        }

        if (mode == &CPU::IMP)
        {
            if (c == 2)
            {
                bus->cpuRead(pc);
                fetched = a;
                (this->*op)();
                if (op == &CPU::STP)
                    jammed = true;
            }
            return;
        }

        if (mode == &CPU::IMM)
        {
            if (c == 2)
            {
                addr_abs = pc;
                fetched = bus->cpuRead(pc++);
                fetchedCached = true;
                (this->*op)();
                fetchedCached = false;
            }
            return;
        }

        if (mode == &CPU::REL)
            return;

        // Endereçamento indireto usado somente por JMP, tratado acima.
        if (mode == &CPU::IND)
            return;

        if (mode == &CPU::ZP0)
        {
            if (c == 2)
                addr_abs = bus->cpuRead(pc++);
            else if (c == 3)
            {
                if (rmw)
                {
                    fetched = bus->cpuRead(addr_abs);
                    fetchedCached = true;
                }
                else if (store)
                    (this->*op)();
                else
                    executeReadOperation();
            }
            else if (rmw && c == 4)
                bus->cpuWrite(addr_abs, fetched);
            else if (rmw && c == 5)
            {
                (this->*op)();
                fetchedCached = false;
            }
            return;
        }

        if (mode == &CPU::ZPX || mode == &CPU::ZPY)
        {
            const uint8_t index = mode == &CPU::ZPX ? x : y;
            if (c == 2)
            {
                zeroPageAddress = bus->cpuRead(pc++);
                baseAddress = zeroPageAddress;
            }
            else if (c == 3)
                bus->cpuRead(zeroPageAddress);
            else if (c == 4)
            {
                addr_abs = static_cast<uint8_t>(zeroPageAddress + index);
                if (rmw)
                {
                    fetched = bus->cpuRead(addr_abs);
                    fetchedCached = true;
                }
                else if (store)
                    (this->*op)();
                else
                    executeReadOperation();
            }
            else if (rmw && c == 5)
                bus->cpuWrite(addr_abs, fetched);
            else if (rmw && c == 6)
            {
                (this->*op)();
                fetchedCached = false;
            }
            return;
        }

        if (mode == &CPU::ABS || mode == &CPU::ABX || mode == &CPU::ABY)
        {
            const uint8_t index = mode == &CPU::ABX ? x : y;
            if (c == 2)
                operandLow = bus->cpuRead(pc++);
            else if (c == 3)
            {
                operandHigh = bus->cpuRead(pc++);
                baseAddress = (static_cast<uint16_t>(operandHigh) << 8) | operandLow;
                addr_abs = mode == &CPU::ABS ? baseAddress : static_cast<uint16_t>(baseAddress + index);
                wrongPageAddress = (baseAddress & 0xFF00) | (addr_abs & 0x00FF);
                pageCrossed = (baseAddress & 0xFF00) != (addr_abs & 0xFF00);
                if (pageCrossed && !store && !rmw)
                    ++cycles;
            }
            else if (mode == &CPU::ABS)
            {
                if (c == 4)
                {
                    if (rmw)
                    {
                        fetched = bus->cpuRead(addr_abs);
                        fetchedCached = true;
                    }
                    else if (store)
                        (this->*op)();
                    else
                        executeReadOperation();
                }
                else if (rmw && c == 5)
                    bus->cpuWrite(addr_abs, fetched);
                else if (rmw && c == 6)
                {
                    (this->*op)();
                    fetchedCached = false;
                }
            }
            else if (rmw)
            {
                if (c == 4)
                    bus->cpuRead(wrongPageAddress);
                else if (c == 5)
                {
                    fetched = bus->cpuRead(addr_abs);
                    fetchedCached = true;
                }
                else if (c == 6)
                    bus->cpuWrite(addr_abs, fetched);
                else if (c == 7)
                {
                    (this->*op)();
                    fetchedCached = false;
                }
            }
            else if (store)
            {
                if (c == 4)
                    bus->cpuRead(wrongPageAddress);
                else if (c == 5)
                    (this->*op)();
            }
            else
            {
                if (c == 4)
                {
                    if (pageCrossed)
                        bus->cpuRead(wrongPageAddress);
                    else
                        executeReadOperation();
                }
                else if (c == 5)
                    executeReadOperation();
            }
            return;
        }

        if (mode == &CPU::IZX)
        {
            if (c == 2)
                zeroPageAddress = bus->cpuRead(pc++);
            else if (c == 3)
                bus->cpuRead(zeroPageAddress);
            else if (c == 4)
                operandLow = bus->cpuRead(static_cast<uint8_t>(zeroPageAddress + x));
            else if (c == 5)
            {
                operandHigh = bus->cpuRead(static_cast<uint8_t>(zeroPageAddress + x + 1));
                addr_abs = (static_cast<uint16_t>(operandHigh) << 8) | operandLow;
            }
            else if (c == 6)
            {
                if (rmw)
                {
                    fetched = bus->cpuRead(addr_abs);
                    fetchedCached = true;
                }
                else if (store)
                    (this->*op)();
                else
                    executeReadOperation();
            }
            else if (rmw && c == 7)
                bus->cpuWrite(addr_abs, fetched);
            else if (rmw && c == 8)
            {
                (this->*op)();
                fetchedCached = false;
            }
            return;
        }

        if (mode == &CPU::IZY)
        {
            if (c == 2)
                zeroPageAddress = bus->cpuRead(pc++);
            else if (c == 3)
                operandLow = bus->cpuRead(zeroPageAddress);
            else if (c == 4)
            {
                operandHigh = bus->cpuRead(static_cast<uint8_t>(zeroPageAddress + 1));
                baseAddress = (static_cast<uint16_t>(operandHigh) << 8) | operandLow;
                addr_abs = static_cast<uint16_t>(baseAddress + y);
                wrongPageAddress = (baseAddress & 0xFF00) | (addr_abs & 0x00FF);
                pageCrossed = (baseAddress & 0xFF00) != (addr_abs & 0xFF00);
                if (pageCrossed && !store && !rmw)
                    ++cycles;
            }
            else if (c == 5)
            {
                if (store || rmw || pageCrossed)
                    bus->cpuRead(wrongPageAddress);
                else
                    executeReadOperation();
            }
            else if (c == 6)
            {
                if (rmw)
                {
                    fetched = bus->cpuRead(addr_abs);
                    fetchedCached = true;
                }
                else if (store)
                    (this->*op)();
                else if (pageCrossed)
                    executeReadOperation();
            }
            else if (rmw && c == 7)
                bus->cpuWrite(addr_abs, fetched);
            else if (rmw && c == 8)
            {
                (this->*op)();
                fetchedCached = false;
            }
            return;
        }
    }

    bool CPU::complete() const
    {
        // True quando não há ciclos pendentes (CPU pronta para nova instrução)
        return cycles == 0;
    }

    void CPU::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&a), sizeof(a));
        os.write(reinterpret_cast<const char *>(&x), sizeof(x));
        os.write(reinterpret_cast<const char *>(&y), sizeof(y));
        os.write(reinterpret_cast<const char *>(&stkp), sizeof(stkp));
        os.write(reinterpret_cast<const char *>(&pc), sizeof(pc));
        os.write(reinterpret_cast<const char *>(&status), sizeof(status));
        os.write(reinterpret_cast<const char *>(&opcode), sizeof(opcode));
        os.write(reinterpret_cast<const char *>(&cycles), sizeof(cycles));
        os.write(reinterpret_cast<const char *>(&fetched), sizeof(fetched));
        os.write(reinterpret_cast<const char *>(&addr_abs), sizeof(addr_abs));
        os.write(reinterpret_cast<const char *>(&addr_rel), sizeof(addr_rel));
        os.write(reinterpret_cast<const char *>(&microCycle), sizeof(microCycle));
        os.write(reinterpret_cast<const char *>(&operandLow), sizeof(operandLow));
        os.write(reinterpret_cast<const char *>(&operandHigh), sizeof(operandHigh));
        os.write(reinterpret_cast<const char *>(&zeroPageAddress), sizeof(zeroPageAddress));
        os.write(reinterpret_cast<const char *>(&baseAddress), sizeof(baseAddress));
        os.write(reinterpret_cast<const char *>(&wrongPageAddress), sizeof(wrongPageAddress));
        os.write(reinterpret_cast<const char *>(&branchTarget), sizeof(branchTarget));
        os.write(reinterpret_cast<const char *>(&pageCrossed), sizeof(pageCrossed));
        os.write(reinterpret_cast<const char *>(&fetchedCached), sizeof(fetchedCached));
        os.write(reinterpret_cast<const char *>(&branchTaken), sizeof(branchTaken));
        os.write(reinterpret_cast<const char *>(&interruptKind), sizeof(interruptKind));
        os.write(reinterpret_cast<const char *>(&interruptRequested), sizeof(interruptRequested));
        os.write(reinterpret_cast<const char *>(&pendingNmi), sizeof(pendingNmi));
        os.write(reinterpret_cast<const char *>(&jammed), sizeof(jammed));
    }

    void CPU::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&a), sizeof(a));
        is.read(reinterpret_cast<char *>(&x), sizeof(x));
        is.read(reinterpret_cast<char *>(&y), sizeof(y));
        is.read(reinterpret_cast<char *>(&stkp), sizeof(stkp));
        is.read(reinterpret_cast<char *>(&pc), sizeof(pc));
        is.read(reinterpret_cast<char *>(&status), sizeof(status));
        is.read(reinterpret_cast<char *>(&opcode), sizeof(opcode));
        is.read(reinterpret_cast<char *>(&cycles), sizeof(cycles));
        is.read(reinterpret_cast<char *>(&fetched), sizeof(fetched));
        is.read(reinterpret_cast<char *>(&addr_abs), sizeof(addr_abs));
        is.read(reinterpret_cast<char *>(&addr_rel), sizeof(addr_rel));
        is.read(reinterpret_cast<char *>(&microCycle), sizeof(microCycle));
        is.read(reinterpret_cast<char *>(&operandLow), sizeof(operandLow));
        is.read(reinterpret_cast<char *>(&operandHigh), sizeof(operandHigh));
        is.read(reinterpret_cast<char *>(&zeroPageAddress), sizeof(zeroPageAddress));
        is.read(reinterpret_cast<char *>(&baseAddress), sizeof(baseAddress));
        is.read(reinterpret_cast<char *>(&wrongPageAddress), sizeof(wrongPageAddress));
        is.read(reinterpret_cast<char *>(&branchTarget), sizeof(branchTarget));
        is.read(reinterpret_cast<char *>(&pageCrossed), sizeof(pageCrossed));
        is.read(reinterpret_cast<char *>(&fetchedCached), sizeof(fetchedCached));
        is.read(reinterpret_cast<char *>(&branchTaken), sizeof(branchTaken));
        is.read(reinterpret_cast<char *>(&interruptKind), sizeof(interruptKind));
        is.read(reinterpret_cast<char *>(&interruptRequested), sizeof(interruptRequested));
        is.read(reinterpret_cast<char *>(&pendingNmi), sizeof(pendingNmi));
        is.read(reinterpret_cast<char *>(&jammed), sizeof(jammed));
    }

    // Busca o dado atual com base no modo de endereçamento calculado
    uint8_t CPU::fetch()
    {
        if (fetchedCached)
            return fetched;
        // Se o modo não for IMP (implied), lê do endereço calculado
        if (!(lookup[opcode].addrmode == &CPU::IMP))
            fetched = bus->cpuRead(addr_abs);
        return fetched;
    }

    // --- Implementações das Instruções de Modo de Endereçamento ---//
    uint8_t CPU::IMP()
    {
        // Modo IMPLIED: o operando está no registrador A
        fetched = a;
        addr_abs = 0x0000;
        return 0;
    }

    uint8_t CPU::IMM()
    {
        // Modo IMMEDIATE: o operando está logo após o opcode
        addr_abs = pc++;
        return 0;
    }

    uint8_t CPU::ZP0()
    {
        // Zero page addressing: lê um offset de 8 bits
        addr_abs = bus->cpuRead(pc++);
        addr_abs &= 0x00FF;
        return 0;
    }

    uint8_t CPU::ZPX()
    {
        // Zero page,X: soma X ao offset de 8 bits (wrap em 0x00FF)
        addr_abs = (bus->cpuRead(pc++) + x);
        addr_abs &= 0x00FF;
        return 0;
    }

    uint8_t CPU::ZPY()
    {
        addr_abs = (bus->cpuRead(pc++) + y);
        addr_abs &= 0x00FF;
        return 0;
    }

    uint8_t CPU::REL()
    {
        // Endereçamento relativo usado por branches.
        // Lê um offset de 8 bits e sign-extend para 16 bits.
        addr_rel = (uint16_t)(int8_t)bus->cpuRead(pc++);
        return 0;
    }

    uint8_t CPU::ABS()
    {
        // Endereçamento absoluto: 16-bit address immediate (lo, hi)
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        return 0;
    }

    uint8_t CPU::ABX()
    {
        // Absolute,X: calcula endereço e detecta cruzamento de página
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        addr_abs += x;

        if ((addr_abs & 0xFF00) != (static_cast<uint16_t>(hi) << 8))
            return 1; // Cruzou a página!
        return 0;
    }

    uint8_t CPU::ABY()
    {
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        addr_abs += y;

        if ((addr_abs & 0xFF00) != (static_cast<uint16_t>(hi) << 8))
            return 1; // Cruzou a página!
        return 0;
    }

    uint8_t CPU::IND()
    {
        // Indirect addressing: usado apenas pelo JMP (JMP (addr)).
        // Reproduz o bug do 6502 quando o low byte é 0xFF.
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        uint16_t ptr = (static_cast<uint16_t>(hi) << 8) | lo;

        if (lo == 0xFF)
            addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr & 0xFF00)) << 8) | bus->cpuRead(ptr);
        else
            addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr + 1)) << 8) | bus->cpuRead(ptr);

        return 0;
    }

    uint8_t CPU::IZX()
    {
        // (Indirect,X) — calcula tabela na pagina zero com X offset
        uint8_t t = bus->cpuRead(pc++);
        uint8_t lo = bus->cpuRead(static_cast<uint8_t>(t + x));
        uint8_t hi = bus->cpuRead(static_cast<uint8_t>(t + x + 1));
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        return 0;
    }

    uint8_t CPU::IZY()
    {
        // (Indirect),Y — lê endereço na página zero e soma Y. Detecta cruzamento.
        uint8_t t = bus->cpuRead(pc++);
        uint8_t lo = bus->cpuRead(t);
        uint8_t hi = bus->cpuRead(static_cast<uint8_t>(t + 1));
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        addr_abs += y;

        if ((addr_abs & 0xFF00) != (static_cast<uint16_t>(hi) << 8))
            return 1; // Cruzou a página!
        return 0;
    }

    uint8_t CPU::ADC()
    {
        fetch();
        // Soma em 16 bits: A + M + C
        uint16_t temp = static_cast<uint16_t>(a) + static_cast<uint16_t>(fetched) + static_cast<uint16_t>(GetFlag(C));

        // Carry: setado se o resultado ultrapassar 255
        SetFlag(C, temp > 255);
        // Overflow: setado se os sinais dos inputs eram iguais, mas o sinal do resultado é diferente
        SetFlag(V, (~(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(fetched)) & (static_cast<uint16_t>(a) ^ temp)) & 0x0080);

        a = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(a);
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::AND()
    {
        fetch();
        a &= fetched;
        updateNZFlags(a);
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::ASL()
    {
        fetch();
        SetFlag(C, fetched & 0x80); // Bit 7 vai para o Carry
        uint8_t temp = fetched << 1;
        updateNZFlags(temp);
        if (lookup[opcode].addrmode == &CPU::IMP)
            a = temp;
        else
            bus->cpuWrite(addr_abs, temp);
        return 0; // ASL (RMW) nunca tem penalidade de ciclo extra
    }

    uint8_t CPU::BCC()
    {
        if (GetFlag(C) == 0)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BCS()
    {
        if (GetFlag(C) == 1)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BEQ()
    {
        if (GetFlag(Z) == 1)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BIT()
    {
        fetch();
        SetFlag(Z, (a & fetched) == 0x00);
        SetFlag(N, fetched & 0x80);
        SetFlag(V, fetched & 0x40);
        return 0;
    }

    uint8_t CPU::BMI()
    {
        if (GetFlag(N) == 1)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BNE()
    {
        if (GetFlag(Z) == 0)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BPL()
    {
        if (GetFlag(N) == 0)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BRK()
    {
        pc++; // BRK pula o byte seguinte (padding)

        push((pc >> 8) & 0x00FF);
        push(pc & 0x00FF);

        // Ao contrário da interrupção de hardware, a interrupção por software (BRK)
        // empilha o status com os bits 4 e 5 (B e U) setados.
        uint8_t status_to_push = status;
        status_to_push |= B;
        status_to_push |= U;
        push(status_to_push);

        SetFlag(I, true); // Desabilita interrupções após o salto

        uint16_t lo = bus->cpuRead(0xFFFE);
        uint16_t hi = bus->cpuRead(0xFFFF);
        pc = (hi << 8) | lo;
        return 0;
    }

    uint8_t CPU::BVC()
    {
        if (GetFlag(V) == 0)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BVS()
    {
        if (GetFlag(V) == 1)
        {
            addr_abs = pc + addr_rel;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::CLC()
    {
        SetFlag(C, false);
        return 0;
    }

    uint8_t CPU::CLD()
    {
        SetFlag(D, false);
        return 0;
    }

    uint8_t CPU::CLI()
    {
        SetFlag(I, false);
        return 0;
    }

    uint8_t CPU::CLV()
    {
        SetFlag(V, false);
        return 0;
    }

    uint8_t CPU::CMP()
    {
        fetch();
        uint16_t temp = static_cast<uint16_t>(a) - static_cast<uint16_t>(fetched);
        SetFlag(C, a >= fetched);
        updateNZFlags(static_cast<uint8_t>(temp & 0x00FF));
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::CPX()
    {
        fetch();
        uint16_t temp = static_cast<uint16_t>(x) - static_cast<uint16_t>(fetched);
        SetFlag(C, x >= fetched);
        updateNZFlags(static_cast<uint8_t>(temp & 0x00FF));
        return 0;
    }

    uint8_t CPU::CPY()
    {
        fetch();
        uint16_t temp = static_cast<uint16_t>(y) - static_cast<uint16_t>(fetched);
        SetFlag(C, y >= fetched);
        updateNZFlags(static_cast<uint8_t>(temp & 0x00FF));
        return 0;
    }

    uint8_t CPU::DEC()
    {
        fetch();
        uint8_t temp = fetched - 1;
        bus->cpuWrite(addr_abs, temp);
        updateNZFlags(temp);
        return 0;
    }

    uint8_t CPU::DEX()
    {
        x--;
        updateNZFlags(x);
        return 0;
    }

    uint8_t CPU::DEY()
    {
        y--;
        updateNZFlags(y);
        return 0;
    }

    uint8_t CPU::EOR()
    {
        fetch();
        a ^= fetched;
        updateNZFlags(a);
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::INC()
    {
        fetch();
        uint8_t temp = fetched + 1;
        bus->cpuWrite(addr_abs, temp);
        updateNZFlags(temp);
        return 0;
    }

    uint8_t CPU::INX()
    {
        x++;
        updateNZFlags(x);
        return 0;
    }

    uint8_t CPU::INY()
    {
        y++;
        updateNZFlags(y);
        return 0;
    }

    uint8_t CPU::JMP()
    {
        // JMP apenas define o PC para o endereço calculado pelo modo de endereçamento
        pc = addr_abs;
        return 0;
    }

    uint8_t CPU::JSR()
    {
        // JSR empilha o endereço do último byte da instrução (PC - 1).
        // O modo ABS() já leu os 2 bytes do operando, então pc aponta para a próxima instrução.
        pc--;

        // Empilha o MSB primeiro, depois o LSB
        push((pc >> 8) & 0x00FF);
        push(pc & 0x00FF);

        pc = addr_abs;
        return 0;
    }

    uint8_t CPU::RTS()
    {
        // Recupera o endereço de retorno da pilha (LSB depois MSB)
        uint16_t lo = static_cast<uint16_t>(pop());
        uint16_t hi = static_cast<uint16_t>(pop());

        // Como o JSR empilhou o endereço - 1, somamos 1 ao retornar
        pc = (hi << 8) | lo;
        pc++;
        return 0;
    }

    uint8_t CPU::LDA()
    {
        fetch();
        a = fetched;
        updateNZFlags(a);
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::LDX()
    {
        fetch();
        x = fetched;
        updateNZFlags(x);
        return 1;
    }

    uint8_t CPU::LDY()
    {
        fetch();
        y = fetched;
        updateNZFlags(y);
        return 1;
    }

    uint8_t CPU::LSR()
    {
        fetch();
        SetFlag(C, fetched & 0x01); // Bit 0 vai para o Carry
        uint8_t temp = fetched >> 1;
        updateNZFlags(temp);
        if (lookup[opcode].addrmode == &CPU::IMP)
            a = temp;
        else
            bus->cpuWrite(addr_abs, temp);
        return 0; // LSR nunca tem penalidade de ciclo extra
    }

    uint8_t CPU::ORA()
    {
        fetch();
        a |= fetched;
        updateNZFlags(a);
        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::PHA()
    {
        push(a);
        return 0;
    }

    uint8_t CPU::PHP()
    {
        // No hardware real, PHP e BRK sempre setam os bits 4 e 5 ao empurrar o status para a pilha
        push(status | B | U);
        return 0;
    }

    uint8_t CPU::PLA()
    {
        a = pop();
        updateNZFlags(a);
        return 0;
    }

    uint8_t CPU::PLP()
    {
        status = pop();
        // Garante que a flag U seja sempre 1 e limpa a flag B (ela só existe na pilha)
        SetFlag(U, true);
        SetFlag(B, false);
        return 0;
    }

    uint8_t CPU::ROL()
    {
        fetch();
        // O novo bit 0 será o Carry atual. O novo Carry será o bit 7 original.
        uint16_t temp = (static_cast<uint16_t>(fetched) << 1) | GetFlag(C);

        SetFlag(C, temp & 0x0100);
        uint8_t result = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(result);

        if (lookup[opcode].addrmode == &CPU::IMP)
            a = result;
        else
            bus->cpuWrite(addr_abs, result);
        return 0;
    }

    uint8_t CPU::ROR()
    {
        fetch();
        // O novo bit 7 será o Carry atual. O novo Carry será o bit 0 original.
        uint8_t old_carry = GetFlag(C);
        SetFlag(C, fetched & 0x01);
        uint8_t result = (fetched >> 1) | (old_carry << 7);
        updateNZFlags(result);

        if (lookup[opcode].addrmode == &CPU::IMP)
            a = result;
        else
            bus->cpuWrite(addr_abs, result);
        return 0;
    }

    uint8_t CPU::RTI()
    {
        // Restaura o registrador de status da pilha
        status = pop();

        // Garante que a flag B seja limpa e a flag U (Unused) seja sempre 1
        SetFlag(B, false);
        SetFlag(U, true);

        // Restaura o Program Counter (LSB depois MSB)
        uint16_t lo = static_cast<uint16_t>(pop());
        uint16_t hi = static_cast<uint16_t>(pop());

        pc = (hi << 8) | lo;
        return 0;
    }

    uint8_t CPU::SBC()
    {
        fetch();

        // No 6502, a subtração é feita usando a lógica de adição:
        // A - M - (1 - C)  é o mesmo que  A + (~M) + C
        // Invertemos os bits do valor buscado (complemento de 1)
        uint16_t value = static_cast<uint16_t>(fetched) ^ 0x00FF;

        // Realizamos a soma em 16 bits para capturar o Carry e o Overflow
        uint16_t temp = static_cast<uint16_t>(a) + value + static_cast<uint16_t>(GetFlag(C));

        // Flag de Carry (C): Na subtração, funciona como "Not Borrow"
        SetFlag(C, temp & 0xFF00);

        // Flag de Overflow (V): Setada se o sinal do resultado for impossível
        // (a ^ temp) & (value ^ temp) & 0x0080
        SetFlag(V, (temp ^ static_cast<uint16_t>(a)) & (temp ^ value) & 0x0080);

        a = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(a);

        return 1; // Permite ciclo extra em cruzamento de página
    }

    uint8_t CPU::SEC()
    {
        SetFlag(C, true);
        return 0;
    }

    uint8_t CPU::SED()
    {
        SetFlag(D, true);
        return 0;
    }

    uint8_t CPU::STA()
    {
        bus->cpuWrite(addr_abs, a);
        return 0;
    }

    uint8_t CPU::SLO()
    {
        fetch();
        // Parte ASL: Shift no valor lido e define o Carry com o bit 7 original
        SetFlag(C, fetched & 0x80);
        fetched <<= 1;
        // Escreve o valor modificado de volta na memória
        bus->cpuWrite(addr_abs, fetched);
        // Parte ORA: OR entre Acumulador e o valor deslocado
        a |= fetched;
        updateNZFlags(a);
        return 0; // SLO é uma instrução RMW e não possui penalidade de ciclo extra por página
    }

    uint8_t CPU::STX()
    {
        bus->cpuWrite(addr_abs, x);
        return 0;
    }

    uint8_t CPU::STY()
    {
        bus->cpuWrite(addr_abs, y);
        return 0;
    }

    uint8_t CPU::TAX()
    {
        x = a;
        updateNZFlags(x);
        return 0;
    }

    uint8_t CPU::TAY()
    {
        y = a;
        updateNZFlags(y);
        return 0;
    }

    uint8_t CPU::TSX()
    {
        x = stkp;
        updateNZFlags(x);
        return 0;
    }

    uint8_t CPU::TXA()
    {
        a = x;
        updateNZFlags(a);
        return 0;
    }

    uint8_t CPU::TXS()
    {
        // TXS é uma das únicas transferências que NÃO altera flags
        stkp = x;
        return 0;
    }

    uint8_t CPU::TYA()
    {
        a = y;
        updateNZFlags(a);
        return 0;
    }

    uint8_t CPU::SEI()
    {
        SetFlag(I, true);
        return 0;
    }

    uint8_t CPU::NOP()
    {
        // O NOP oficial não tem cross-page, mas NOPs ilegais (como $1C) têm.
        // Como o modo IMP retorna 0, retornar 1 aqui é seguro e preciso.
        return 1;
    }

    uint8_t CPU::LAX()
    {
        fetch();
        a = fetched;
        x = fetched;
        updateNZFlags(a);
        return 1; // Pode ter ciclo extra se o modo de endereçamento permitir
    }

    uint8_t CPU::SAX()
    {
        // Armazena (A AND X) na memória
        bus->cpuWrite(addr_abs, a & x);
        return 0;
    }

    uint8_t CPU::DCP()
    {
        // Decrementa memória e depois compara com A
        fetch();
        fetched--;
        bus->cpuWrite(addr_abs, fetched);
        if (a >= fetched)
            SetFlag(C, true);
        else
            SetFlag(C, false);
        updateNZFlags(a - fetched);
        return 0;
    }

    uint8_t CPU::STP()
    {
        // Trava a CPU: o Program Counter retrocede um byte para apontar novamente para o opcode STP.
        // Isso cria um loop efetivo onde a CPU continuará lendo e executando a mesma instrução.
        // No hardware real, apenas um sinal de RESET pode tirar a CPU deste estado.
        pc--;
        return 0;
    }

    uint8_t CPU::ANC()
    {
        fetch();
        a &= fetched;
        updateNZFlags(a);
        // O Carry é definido como o valor do bit 7 do Acumulador (igual ao Negative flag)
        SetFlag(C, a & 0x80);

        return 0; // ANC utiliza apenas o modo IMM, que não possui penalidade de ciclo
    }

    uint8_t CPU::RLA()
    {
        // 1. Busca o valor da memória (conforme o modo de endereçamento calculado)
        fetch();

        // 2. Operação ROL (Rotate Left)
        // O bit 7 original vai para o Carry, e o Carry antigo vai para o bit 0.
        uint16_t temp = (static_cast<uint16_t>(fetched) << 1) | GetFlag(C);
        SetFlag(C, temp & 0x0100);
        uint8_t rotated = static_cast<uint8_t>(temp & 0x00FF);

        // 3. Escreve o valor rotacionado de volta na memória
        bus->cpuWrite(addr_abs, rotated);

        // 4. Executa o AND entre o Acumulador e o resultado da rotação
        a &= rotated;

        // 5. Atualiza flags baseadas no Acumulador
        updateNZFlags(a);

        // RLA é uma instrução RMW; não costuma ter penalidade de ciclo por cruzamento de página.
        return 0;
    }

    uint8_t CPU::SRE()
    {
        // 1. Busca o valor da memória
        fetch();

        // 2. Operação LSR (Logical Shift Right)
        // O bit 0 original vai para o Carry
        SetFlag(C, fetched & 0x01);
        fetched >>= 1;

        // 3. Escreve o valor modificado de volta na memória
        bus->cpuWrite(addr_abs, fetched);

        // 4. Operação EOR (Exclusive OR) entre Acumulador e o valor deslocado
        a ^= fetched;
        updateNZFlags(a);

        return 0; // Instruções RMW não possuem penalidade de ciclo extra por cruzamento de página
    }

    uint8_t CPU::RRA()
    {
        fetch();

        // 1. Parte ROR: Rotaciona o valor lido para a direita
        uint8_t old_carry = GetFlag(C);
        SetFlag(C, fetched & 0x01); // O bit 0 original vai para o Carry
        fetched = (fetched >> 1) | (old_carry << 7);

        // 2. Escreve o valor rotacionado de volta na memória
        bus->cpuWrite(addr_abs, fetched);

        // 3. Parte ADC: Adiciona o resultado da rotação ao Acumulador
        // Importante: Usamos o Carry que foi definido pela operação de rotação acima!
        uint16_t temp = static_cast<uint16_t>(a) + static_cast<uint16_t>(fetched) + static_cast<uint16_t>(GetFlag(C));
        SetFlag(V, (~(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(fetched)) & (static_cast<uint16_t>(a) ^ temp)) & 0x0080);
        SetFlag(C, temp > 0x00FF);
        a = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(a);

        return 0; // Instrução RMW, não possui penalidade de ciclo extra
    }

    uint8_t CPU::ISC()
    {
        // 1. Parte INC: Busca o dado, incrementa e salva de volta
        fetch();
        fetched++;
        bus->cpuWrite(addr_abs, fetched);

        // 2. Parte SBC: Subtrai o valor incrementado do Acumulador
        // Usamos a mesma lógica implementada no seu método SBC()
        uint16_t value = static_cast<uint16_t>(fetched) ^ 0x00FF;
        uint16_t temp = static_cast<uint16_t>(a) + value + static_cast<uint16_t>(GetFlag(C));
        SetFlag(C, temp & 0xFF00);
        SetFlag(V, (temp ^ static_cast<uint16_t>(a)) & (temp ^ value) & 0x0080);
        a = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(a);

        return 0; // Instrução RMW não tem penalidade de ciclo extra por cruzamento de página
    }

    uint8_t CPU::ALR()
    {
        fetch();
        a &= fetched;
        // O bit 0 do resultado do AND vai para o Carry antes do shift
        SetFlag(C, a & 0x01);
        a >>= 1;
        updateNZFlags(a);
        return 0;
    }

    uint8_t CPU::ARR()
    {
        fetch();
        a &= fetched;

        uint8_t old_carry = GetFlag(C);
        // Executa a rotação para a direita
        a = (a >> 1) | (old_carry << 7);

        // Atualiza flags Negative e Zero baseadas no resultado final
        updateNZFlags(a);

        // Flags específicas da instrução ARR no modo não-decimal:
        // Carry é definido como o bit 6 do resultado
        SetFlag(C, (a >> 6) & 0x01);
        // Overflow é o bit 6 XOR bit 5 do resultado
        SetFlag(V, ((a >> 6) ^ (a >> 5)) & 0x01);

        return 0;
    }

    uint8_t CPU::XAA()
    {
        fetch();
        // Implementação estável: A = X AND imediato
        a = x & fetched;
        updateNZFlags(a);
        return 0;
    }

    uint8_t CPU::SHA()
    {
        // Armazena A & X & (High Byte do endereço + 1)
        uint8_t val = a & x & (uint8_t)((addr_abs >> 8) + 1);
        bus->cpuWrite(addr_abs, val);
        return 0;
    }

    uint8_t CPU::SHY()
    {
        // Armazena Y & (High Byte do endereço + 1)
        uint8_t val = y & (uint8_t)((addr_abs >> 8) + 1);
        bus->cpuWrite(addr_abs, val);
        return 0;
    }

    uint8_t CPU::SHX()
    {
        // Armazena X & (High Byte do endereço + 1)
        uint8_t val = x & (uint8_t)((addr_abs >> 8) + 1);
        bus->cpuWrite(addr_abs, val);
        return 0;
    }

    uint8_t CPU::TAS()
    {
        // S = A & X, depois armazena S & (High Byte do endereço + 1)
        stkp = a & x;
        uint8_t val = stkp & (uint8_t)((addr_abs >> 8) + 1);
        bus->cpuWrite(addr_abs, val);
        return 0;
    }

    uint8_t CPU::LAS()
    {
        fetch();
        // A = M AND S
        a = fetched & stkp;
        // S = M AND S
        stkp = a; // Stack Pointer recebe o mesmo valor que A
        updateNZFlags(a);

        return 1;
    }

    uint8_t CPU::SBX()
    {
        fetch();
        // (A AND X) - M
        uint16_t temp = (static_cast<uint16_t>(a & x)) - static_cast<uint16_t>(fetched);

        // Flag C: setada se (A & X) >= M
        SetFlag(C, (a & x) >= fetched);

        x = static_cast<uint8_t>(temp & 0x00FF);
        updateNZFlags(x);

        return 0;
    }

    std::map<uint16_t, std::string> CPU::disassemble(uint16_t nStart, uint16_t nStop)
    {
        uint32_t addr = nStart;
        uint8_t value = 0x00, lo = 0x00, hi = 0x00;
        std::map<uint16_t, std::string> mapLines;
        uint16_t line_addr = 0;

        // Helper para converter números em Hexadecimal formatado
        auto hex = [](uint32_t n, uint8_t d)
        {
            std::string s(d, '0');
            for (int i = d - 1; i >= 0; i--, n >>= 4)
                s[i] = "0123456789ABCDEF"[n & 0xF];
            return s;
        };

        while (addr <= (uint32_t)nStop)
        {
            line_addr = (uint16_t)addr;
            std::string sInst = "$" + hex(line_addr, 4) + ": ";
            uint8_t opcode = bus->cpuRead(addr);
            addr++;

            sInst += lookup[opcode].name + " ";

            if (lookup[opcode].addrmode == &CPU::IMP)
            {
                sInst += " {IMP}";
            }
            else if (lookup[opcode].addrmode == &CPU::IMM)
            {
                value = bus->cpuRead(addr++);
                sInst += "#$" + hex(value, 2) + " {IMM}";
            }
            else if (lookup[opcode].addrmode == &CPU::ZP0)
            {
                lo = bus->cpuRead(addr++);
                sInst += "$" + hex(lo, 2) + " {ZP0}";
            }
            else if (lookup[opcode].addrmode == &CPU::ZPX)
            {
                lo = bus->cpuRead(addr++);
                sInst += "$" + hex(lo, 2) + ", X {ZPX}";
            }
            else if (lookup[opcode].addrmode == &CPU::ZPY)
            {
                lo = bus->cpuRead(addr++);
                sInst += "$" + hex(lo, 2) + ", Y {ZPY}";
            }
            else if (lookup[opcode].addrmode == &CPU::IZX)
            {
                lo = bus->cpuRead(addr++);
                sInst += "($" + hex(lo, 2) + ", X) {IZX}";
            }
            else if (lookup[opcode].addrmode == &CPU::IZY)
            {
                lo = bus->cpuRead(addr++);
                sInst += "($" + hex(lo, 2) + "), Y {IZY}";
            }
            else if (lookup[opcode].addrmode == &CPU::ABS)
            {
                lo = bus->cpuRead(addr++);
                hi = bus->cpuRead(addr++);
                sInst += "$" + hex((uint16_t)(hi << 8) | lo, 4) + " {ABS}";
            }
            else if (lookup[opcode].addrmode == &CPU::ABX)
            {
                lo = bus->cpuRead(addr++);
                hi = bus->cpuRead(addr++);
                sInst += "$" + hex((uint16_t)(hi << 8) | lo, 4) + ", X {ABX}";
            }
            else if (lookup[opcode].addrmode == &CPU::ABY)
            {
                lo = bus->cpuRead(addr++);
                hi = bus->cpuRead(addr++);
                sInst += "$" + hex((uint16_t)(hi << 8) | lo, 4) + ", Y {ABY}";
            }
            else if (lookup[opcode].addrmode == &CPU::IND)
            {
                lo = bus->cpuRead(addr++);
                hi = bus->cpuRead(addr++);
                sInst += "($" + hex((uint16_t)(hi << 8) | lo, 4) + ") {IND}";
            }
            else if (lookup[opcode].addrmode == &CPU::REL)
            {
                value = bus->cpuRead(addr++);
                sInst += "$" + hex(value, 2) + " [$" + hex(addr + (int8_t)value, 4) + "] {REL}";
            }

            mapLines[line_addr] = sInst;
        }

        return mapLines;
    }
}