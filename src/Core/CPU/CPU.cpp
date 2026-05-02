#include "Core/CPU/CPU.h"
#include "Core/Bus/Bus.h"
#include <iomanip>
#include <sstream>

namespace R2NES::Core
{
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
            {"SHA", &CPU::XXX, &CPU::IZY, 6},
            {"STY", &CPU::STY, &CPU::ZPX, 4},
            {"STA", &CPU::STA, &CPU::ZPX, 4},
            {"STX", &CPU::STX, &CPU::ZPY, 4},
            {"SAX", &CPU::SAX, &CPU::ZPY, 4},
            {"TYA", &CPU::TYA, &CPU::IMP, 2},
            {"STA", &CPU::STA, &CPU::ABY, 5},
            {"TXS", &CPU::TXS, &CPU::IMP, 2},
            {"TAS", &CPU::XXX, &CPU::ABY, 5},
            {"SHY", &CPU::XXX, &CPU::ABX, 5},
            {"STA", &CPU::STA, &CPU::ABX, 5},
            {"SHX", &CPU::XXX, &CPU::ABY, 5},
            {"SHA", &CPU::XXX, &CPU::ABY, 5},
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
            {"LAS", &CPU::XXX, &CPU::ABY, 4},
            {"LDY", &CPU::LDY, &CPU::ABX, 4},
            {"LDA", &CPU::LDA, &CPU::ABX, 4},
            {"LDX", &CPU::LDX, &CPU::ABY, 4},
            {"LAX", &CPU::LAX, &CPU::ABY, 4},
            {"CPY", &CPU::CPY, &CPU::IMM, 2},
            {"CMP", &CPU::CMP, &CPU::IZX, 6},
            {"NOP", &CPU::NOP, &CPU::IMM, 2},
            {"DCP", &CPU::DCP, &CPU::IZX, 8},
            {"CPY", &CPU::CPY, &CPU::ZP0, 3},
            {"CMP", &CPU::CMP, &CPU::ZP0, 3},
            {"DEC", &CPU::DEC, &CPU::ZP0, 5},
            {"DCP", &CPU::DCP, &CPU::ZP0, 5},
            {"INY", &CPU::INY, &CPU::IMP, 2},
            {"CMP", &CPU::CMP, &CPU::IMM, 2},
            {"DEX", &CPU::DEX, &CPU::IMP, 2},
            {"SBX", &CPU::XXX, &CPU::IMM, 2},
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
            {"ISC", &CPU::ISC, &CPU::IZX, 8},
            {"CPX", &CPU::CPX, &CPU::ZP0, 3},
            {"SBC", &CPU::SBC, &CPU::ZP0, 3},
            {"INC", &CPU::INC, &CPU::ZP0, 5},
            {"ISC", &CPU::ISC, &CPU::ZP0, 5},
            {"INX", &CPU::INX, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::IMM, 2},
            {"CPX", &CPU::CPX, &CPU::ABS, 4},
            {"SBC", &CPU::SBC, &CPU::ABS, 4},
            {"INC", &CPU::INC, &CPU::ABS, 6},
            {"ISC", &CPU::ISC, &CPU::ABS, 6},
            {"BEQ", &CPU::BEQ, &CPU::REL, 2},
            {"SBC", &CPU::SBC, &CPU::IZY, 5},
            {"STP", &CPU::STP, &CPU::IMP, 2},
            {"ISC", &CPU::ISC, &CPU::IZY, 8},
            {"NOP", &CPU::NOP, &CPU::ZPX, 4},
            {"SBC", &CPU::SBC, &CPU::ZPX, 4},
            {"INC", &CPU::INC, &CPU::ZPX, 6},
            {"ISC", &CPU::ISC, &CPU::ZPX, 6},
            {"SED", &CPU::SED, &CPU::IMP, 2},
            {"SBC", &CPU::SBC, &CPU::ABY, 4},
            {"NOP", &CPU::NOP, &CPU::IMP, 2},
            {"ISC", &CPU::ISC, &CPU::ABY, 7},
            {"NOP", &CPU::NOP, &CPU::ABX, 4},
            {"SBC", &CPU::SBC, &CPU::ABX, 4},
            {"INC", &CPU::INC, &CPU::ABX, 7},
            {"ISC", &CPU::ISC, &CPU::ABX, 7},
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

    bool CPU::complete() const
    {
        return cycles == 0;
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
        if (lo == 0xFF)
            addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr & 0xFF00)) << 8) | bus->cpuRead(ptr);
        else
            addr_abs = (static_cast<uint16_t>(bus->cpuRead(ptr + 1)) << 8) | bus->cpuRead(ptr);

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
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BCS()
    {
        if (GetFlag(C) == 1)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BEQ()
    {
        if (GetFlag(Z) == 1)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
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
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BNE()
    {
        if (GetFlag(Z) == 0)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BPL()
    {
        if (GetFlag(N) == 0)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BRK()
    {
        pc++; // BRK pula o byte seguinte (padding)

        SetFlag(I, true);
        push((pc >> 8) & 0x00FF);
        push(pc & 0x00FF);

        // BRK empilha o status com o bit B (Break) e U (Unused) setados
        SetFlag(B, true);
        push(status);
        SetFlag(B, false); // O bit B não existe fisicamente no registrador status

        uint16_t lo = bus->cpuRead(0xFFFE);
        uint16_t hi = bus->cpuRead(0xFFFF);
        pc = (hi << 8) | lo;
        return 0;
    }

    uint8_t CPU::BVC()
    {
        if (GetFlag(V) == 0)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
            pc = addr_abs;
        }
        return 0;
    }

    uint8_t CPU::BVS()
    {
        if (GetFlag(V) == 1)
        {
            cycles++;
            addr_abs = pc + addr_rel;
            if ((addr_abs & 0xFF00) != (pc & 0xFF00))
                cycles++;
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

    uint8_t CPU::XXX()
    {
        // Placeholder para opcodes ilegais.
        // Retornar 1 permite que versões ilegais de instruções aproveitem o cross-page.
        return 1;
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
}