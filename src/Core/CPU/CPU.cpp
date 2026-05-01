#include "Core/CPU/CPU.h"
#include "Core/Bus/Bus.h"

namespace R2NES::Core
{
    CPU::CPU()
    {
        // Inicializa a tabela de opcodes (256 entradas)
        // Formato: { "NOME", Operação, Modo de Endereçamento, Ciclos base }
        lookup = {
            {"BRK", &CPU::BRK, &CPU::IMM, 7},
            {"ORA", &CPU::ORA, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 3},
            {"ORA", &CPU::ORA, &CPU::ZP0, 3},
            {"ASL", &CPU::ASL, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"PHP", &CPU::PHP, &CPU::IMP, 3},
            {"ORA", &CPU::ORA, &CPU::IMM, 2},
            {"ASL", &CPU::ASL, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"ORA", &CPU::ORA, &CPU::ABS, 4},
            {"ASL", &CPU::ASL, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BPL", &CPU::BPL, &CPU::REL, 2},
            {"ORA", &CPU::ORA, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"ORA", &CPU::ORA, &CPU::ZPX, 4},
            {"ASL", &CPU::ASL, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"CLC", &CPU::CLC, &CPU::IMP, 2},
            {"ORA", &CPU::ORA, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"ORA", &CPU::ORA, &CPU::ABX, 4},
            {"ASL", &CPU::ASL, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"JSR", &CPU::JSR, &CPU::ABS, 6},
            {"AND", &CPU::AND, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"BIT", &CPU::BIT, &CPU::ZP0, 3},
            {"AND", &CPU::AND, &CPU::ZP0, 3},
            {"ROL", &CPU::ROL, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"PLP", &CPU::PLP, &CPU::IMP, 4},
            {"AND", &CPU::AND, &CPU::IMM, 2},
            {"ROL", &CPU::ROL, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"BIT", &CPU::BIT, &CPU::ABS, 4},
            {"AND", &CPU::AND, &CPU::ABS, 4},
            {"ROL", &CPU::ROL, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BMI", &CPU::BMI, &CPU::REL, 2},
            {"AND", &CPU::AND, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"AND", &CPU::AND, &CPU::ZPX, 4},
            {"ROL", &CPU::ROL, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"SEC", &CPU::SEC, &CPU::IMP, 2},
            {"AND", &CPU::AND, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"AND", &CPU::AND, &CPU::ABX, 4},
            {"ROL", &CPU::ROL, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"RTI", &CPU::RTI, &CPU::IMP, 6},
            {"EOR", &CPU::EOR, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 3},
            {"EOR", &CPU::EOR, &CPU::ZP0, 3},
            {"LSR", &CPU::LSR, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"PHA", &CPU::PHA, &CPU::IMP, 3},
            {"EOR", &CPU::EOR, &CPU::IMM, 2},
            {"LSR", &CPU::LSR, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"JMP", &CPU::JMP, &CPU::ABS, 3},
            {"EOR", &CPU::EOR, &CPU::ABS, 4},
            {"LSR", &CPU::LSR, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BVC", &CPU::BVC, &CPU::REL, 2},
            {"EOR", &CPU::EOR, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"EOR", &CPU::EOR, &CPU::ZPX, 4},
            {"LSR", &CPU::LSR, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"CLI", &CPU::CLI, &CPU::IMP, 2},
            {"EOR", &CPU::EOR, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"EOR", &CPU::EOR, &CPU::ABX, 4},
            {"LSR", &CPU::LSR, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"RTS", &CPU::RTS, &CPU::IMP, 6},
            {"ADC", &CPU::ADC, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 3},
            {"ADC", &CPU::ADC, &CPU::ZP0, 3},
            {"ROR", &CPU::ROR, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"PLA", &CPU::PLA, &CPU::IMP, 4},
            {"ADC", &CPU::ADC, &CPU::IMM, 2},
            {"ROR", &CPU::ROR, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"JMP", &CPU::JMP, &CPU::IND, 5},
            {"ADC", &CPU::ADC, &CPU::ABS, 4},
            {"ROR", &CPU::ROR, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BVS", &CPU::BVS, &CPU::REL, 2},
            {"ADC", &CPU::ADC, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"ADC", &CPU::ADC, &CPU::ZPX, 4},
            {"ROR", &CPU::ROR, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"SEI", &CPU::SEI, &CPU::IMP, 2},
            {"ADC", &CPU::ADC, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"ADC", &CPU::ADC, &CPU::ABX, 4},
            {"ROR", &CPU::ROR, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"STA", &CPU::STA, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"STY", &CPU::STY, &CPU::ZP0, 3},
            {"STA", &CPU::STA, &CPU::ZP0, 3},
            {"STX", &CPU::STX, &CPU::ZP0, 3},
            {"XXX", &CPU::XXX, &CPU::IMP, 3},
            {"DEY", &CPU::DEY, &CPU::IMP, 2},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"TXA", &CPU::TXA, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"STY", &CPU::STY, &CPU::ABS, 4},
            {"STA", &CPU::STA, &CPU::ABS, 4},
            {"STX", &CPU::STX, &CPU::ABS, 4},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"BCC", &CPU::BCC, &CPU::REL, 2},
            {"STA", &CPU::STA, &CPU::IZY, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"STY", &CPU::STY, &CPU::ZPX, 4},
            {"STA", &CPU::STA, &CPU::ZPX, 4},
            {"STX", &CPU::STX, &CPU::ZPY, 4},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"TYA", &CPU::TYA, &CPU::IMP, 2},
            {"STA", &CPU::STA, &CPU::ABY, 5},
            {"TXS", &CPU::TXS, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"STA", &CPU::STA, &CPU::ABX, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"LDY", &CPU::LDY, &CPU::IMM, 2},
            {"LDA", &CPU::LDA, &CPU::IZX, 6},
            {"LDX", &CPU::LDX, &CPU::IMM, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"LDY", &CPU::LDY, &CPU::ZP0, 3},
            {"LDA", &CPU::LDA, &CPU::ZP0, 3},
            {"LDX", &CPU::LDX, &CPU::ZP0, 3},
            {"XXX", &CPU::XXX, &CPU::IMP, 3},
            {"TAY", &CPU::TAY, &CPU::IMP, 2},
            {"LDA", &CPU::LDA, &CPU::IMM, 2},
            {"TAX", &CPU::TAX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"LDY", &CPU::LDY, &CPU::ABS, 4},
            {"LDA", &CPU::LDA, &CPU::ABS, 4},
            {"LDX", &CPU::LDX, &CPU::ABS, 4},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"BCS", &CPU::BCS, &CPU::REL, 2},
            {"LDA", &CPU::LDA, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"LDY", &CPU::LDY, &CPU::ZPX, 4},
            {"LDA", &CPU::LDA, &CPU::ZPX, 4},
            {"LDX", &CPU::LDX, &CPU::ZPY, 4},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"CLV", &CPU::CLV, &CPU::IMP, 2},
            {"LDA", &CPU::LDA, &CPU::ABY, 4},
            {"TSX", &CPU::TSX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"LDY", &CPU::LDY, &CPU::ABX, 4},
            {"LDA", &CPU::LDA, &CPU::ABX, 4},
            {"LDX", &CPU::LDX, &CPU::ABY, 4},
            {"XXX", &CPU::XXX, &CPU::IMP, 4},
            {"CPY", &CPU::CPY, &CPU::IMM, 2},
            {"CMP", &CPU::CMP, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"CPY", &CPU::CPY, &CPU::ZP0, 3},
            {"CMP", &CPU::CMP, &CPU::ZP0, 3},
            {"DEC", &CPU::DEC, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"INY", &CPU::INY, &CPU::IMP, 2},
            {"CMP", &CPU::CMP, &CPU::IMM, 2},
            {"DEX", &CPU::DEX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"CPY", &CPU::CPY, &CPU::ABS, 4},
            {"CMP", &CPU::CMP, &CPU::ABS, 4},
            {"DEC", &CPU::DEC, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BNE", &CPU::BNE, &CPU::REL, 2},
            {"CMP", &CPU::CMP, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"CMP", &CPU::CMP, &CPU::ZPX, 4},
            {"DEC", &CPU::DEC, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"CLD", &CPU::CLD, &CPU::IMP, 2},
            {"CMP", &CPU::CMP, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"CMP", &CPU::CMP, &CPU::ABX, 4},
            {"DEC", &CPU::DEC, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"CPX", &CPU::CPX, &CPU::IMM, 2},
            {"SBC", &CPU::SBC, &CPU::IZX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"CPX", &CPU::CPX, &CPU::ZP0, 3},
            {"SBC", &CPU::SBC, &CPU::ZP0, 3},
            {"INC", &CPU::INC, &CPU::ZP0, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 5},
            {"INX", &CPU::INX, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"CPX", &CPU::CPX, &CPU::ABS, 4},
            {"SBC", &CPU::SBC, &CPU::ABS, 4},
            {"INC", &CPU::INC, &CPU::ABS, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"BEQ", &CPU::BEQ, &CPU::REL, 2},
            {"SBC", &CPU::SBC, &CPU::IZY, 5},
            {"XXX", &CPU::XXX, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 8},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"SBC", &CPU::SBC, &CPU::ZPX, 4},
            {"INC", &CPU::INC, &CPU::ZPX, 6},
            {"XXX", &CPU::XXX, &CPU::IMP, 6},
            {"SED", &CPU::SED, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
            {"NOP", &CPU::NOP, &CPU::IMP, 4},
            {"SBC", &CPU::SBC, &CPU::ABX, 4},
            {"INC", &CPU::INC, &CPU::ABX, 7},
            {"XXX", &CPU::XXX, &CPU::IMP, 7},
        };
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

    void CPU::updateNZFlags(uint8_t value)
    {
        SetFlag(Z, value == 0x00);
        SetFlag(N, value & 0x80);
    }

    void CPU::push(uint8_t data)
    {
        bus->cpuWrite(0x0100 + stkp, data);
        stkp--;
    }

    uint8_t CPU::pop()
    {
        stkp++;
        return bus->cpuRead(0x0100 + stkp);
    }

    void CPU::irq()
    {
        if (GetFlag(I) == 0)
        {
            push((pc >> 8) & 0x00FF);
            push(pc & 0x00FF);

            SetFlag(B, false);
            SetFlag(U, true);
            SetFlag(I, true);
            push(status);

            uint16_t lo = bus->cpuRead(0xFFFE);
            uint16_t hi = bus->cpuRead(0xFFFF);
            pc = (hi << 8) | lo;

            cycles = 7;
        }
    }

    void CPU::nmi()
    {
        push((pc >> 8) & 0x00FF);
        push(pc & 0x00FF);

        SetFlag(B, false);
        SetFlag(U, true);
        SetFlag(I, true);
        push(status);

        uint16_t lo = bus->cpuRead(0xFFFA);
        uint16_t hi = bus->cpuRead(0xFFFB);
        pc = (hi << 8) | lo;

        cycles = 8;
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
        if (cycles == 0)
        {
            // Garante que a flag Unused esteja sempre 1
            SetFlag(U, true);

            // 1. Fetch: Lê o opcode no endereço apontado pelo PC
            opcode = bus->cpuRead(pc);
            pc++;

            // Obtém os ciclos base da instrução
            cycles = lookup[opcode].cycles;

            // 2. Decode & Execute
            // Chama o modo de endereçamento. Pode retornar 1 ciclo extra (ex: cross-page)
            uint8_t additional_cycle1 = (this->*lookup[opcode].addrmode)();

            // Chama a operação propriamente dita. Também pode retornar 1 ciclo extra
            uint8_t additional_cycle2 = (this->*lookup[opcode].operate)();

            // Soma ciclos extras apenas se ambos pedirem (lógica específica do 6502 para certas instruções)
            cycles += (additional_cycle1 & additional_cycle2);

            // Garante novamente a flag Unused
            SetFlag(U, true);
        }

        // Decrementa o contador de ciclos
        cycles--;
    }

    // Busca o dado atual com base no modo de endereçamento calculado
    uint8_t CPU::fetch()
    {
        if (!(lookup[opcode].addrmode == &CPU::IMP))
            fetched = bus->cpuRead(addr_abs);
        return fetched;
    }

    // --- Implementações das Instruções de Modo de Endereçamento ---//
    uint8_t CPU::IMP() 
    { 
        fetched = a;
        addr_abs = 0x0000;
        return 0; 
    }

    uint8_t CPU::IMM() 
    { 
        addr_abs = pc++;
        return 0; 
    }

    uint8_t CPU::ZP0() 
    { 
        addr_abs = bus->cpuRead(pc++);
        addr_abs &= 0x00FF;
        return 0; 
    }

    uint8_t CPU::ZPX() 
    { 
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
        // Lê como uint8_t, converte para int8_t para preservar o sinal, e então para uint16_t
        addr_rel = (uint16_t)(int8_t)bus->cpuRead(pc++);
        return 0; 
    }

    uint8_t CPU::ABS() 
    { 
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        return 0; 
    }

    uint8_t CPU::ABX() 
    { 
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
        uint8_t lo = bus->cpuRead(pc++);
        uint8_t hi = bus->cpuRead(pc++);
        uint16_t ptr = (static_cast<uint16_t>(hi) << 8) | lo;

        // Simula o bug do hardware original do 6502 no JMP indireto
        if (lo == 0xFF) addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr & 0xFF00)) << 8) | bus->cpuRead(ptr);
        else addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr + 1)) << 8) | bus->cpuRead(ptr);

        return 0; 
    }

    uint8_t CPU::IZX() 
    { 
        uint8_t t = bus->cpuRead(pc++);
        uint8_t lo = bus->cpuRead(static_cast<uint8_t>(t + x));
        uint8_t hi = bus->cpuRead(static_cast<uint8_t>(t + x + 1));
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        return 0; 
    }

    uint8_t CPU::IZY() 
    { 
        uint8_t t = bus->cpuRead(pc++);
        uint8_t lo = bus->cpuRead(t);
        uint8_t hi = bus->cpuRead(static_cast<uint8_t>(t + 1));
        addr_abs = (static_cast<uint16_t>(hi) << 8) | lo;
        addr_abs += y;

        if ((addr_abs & 0xFF00) != (static_cast<uint16_t>(hi) << 8))
            return 1; // Cruzou a página!
        return 0; 
    }

    uint8_t CPU::ADC() { return 0; }
    uint8_t CPU::AND() { return 0; }
    uint8_t CPU::ASL() { return 0; }
    uint8_t CPU::BCC() { return 0; }
    uint8_t CPU::BCS() { return 0; }
    uint8_t CPU::BEQ() { return 0; }
    uint8_t CPU::BIT() { return 0; }
    uint8_t CPU::BMI() { return 0; }
    uint8_t CPU::BNE() { return 0; }
    uint8_t CPU::BPL() { return 0; }
    uint8_t CPU::BRK() { return 0; }
    uint8_t CPU::BVC() { return 0; }
    uint8_t CPU::BVS() { return 0; }
    uint8_t CPU::CLC() { return 0; }
    uint8_t CPU::CLD() { return 0; }
    uint8_t CPU::CLI() { return 0; }
    uint8_t CPU::CLV() { return 0; }
    uint8_t CPU::CMP() { return 0; }
    uint8_t CPU::CPX() { return 0; }
    uint8_t CPU::CPY() { return 0; }
    uint8_t CPU::DEC() { return 0; }
    uint8_t CPU::DEX() { return 0; }
    uint8_t CPU::DEY() { return 0; }
    uint8_t CPU::EOR() { return 0; }
    uint8_t CPU::INC() { return 0; }
    uint8_t CPU::INX() { return 0; }
    uint8_t CPU::INY() { return 0; }
    uint8_t CPU::JMP() { return 0; }
    uint8_t CPU::JSR() { return 0; }
    uint8_t CPU::LDA() { return 0; }
    uint8_t CPU::LDX() { return 0; }
    uint8_t CPU::LDY() { return 0; }
    uint8_t CPU::LSR() { return 0; }
    uint8_t CPU::ORA() { return 0; }
    uint8_t CPU::PHA() { return 0; }
    uint8_t CPU::PHP() { return 0; }
    uint8_t CPU::PLA() { return 0; }
    uint8_t CPU::PLP() { return 0; }
    uint8_t CPU::ROL() { return 0; }
    uint8_t CPU::ROR() { return 0; }
    uint8_t CPU::RTI() { return 0; }
    uint8_t CPU::RTS() { return 0; }
    uint8_t CPU::SBC() { return 0; }
    uint8_t CPU::SEC() { return 0; }
    uint8_t CPU::SED() { return 0; }
    uint8_t CPU::STA() { return 0; }
    uint8_t CPU::STX() { return 0; }
    uint8_t CPU::STY() { return 0; }
    uint8_t CPU::TAX() { return 0; }
    uint8_t CPU::TAY() { return 0; }
    uint8_t CPU::TSX() { return 0; }
    uint8_t CPU::TXA() { return 0; }
    uint8_t CPU::TXS() { return 0; }
    uint8_t CPU::TYA() { return 0; }
    uint8_t CPU::XXX() { return 0; }
    uint8_t CPU::SEI()
    {
        SetFlag(I, true);
        return 0;
    }
    uint8_t CPU::NOP() { return 0; }
}