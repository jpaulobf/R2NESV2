#include "Core/Cartridge/Mappers/Mapper005.h"
#include "Core/NES.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace R2NES::Core;
static void check(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}
static void write(Mapper005 &mapper, uint16_t address, uint8_t value)
{
    uint32_t mapped = 0;
    mapper.cpuMapWrite(address, mapped, value, 0);
}
static uint32_t ppuRead(Mapper005 &mapper, uint16_t address, PpuReadContext context, uint8_t &data)
{
    uint32_t mapped = 0;
    mapper.setPpuReadContext(context);
    check(mapper.ppuMapRead(address, mapped, data, 0), "CHR read rejected");
    mapper.setPpuReadContext(PpuReadContext::Cpu);
    return mapped;
}
static uint32_t chr(Mapper005 &mapper, uint16_t address, bool sprite)
{
    uint8_t data = 0;
    return ppuRead(mapper, address, sprite ? PpuReadContext::Sprite : PpuReadContext::Background, data);
}
static std::string state(Mapper &mapper)
{
    std::stringstream output;
    mapper.saveState(output);
    check(output.good(), "Mapper save failed");
    return output.str();
}
static unsigned captured(unsigned mode, unsigned high, unsigned data)
{
    // Matriz de conexões da tabela $512x do NesDev: 0..7 = CPU D0..D7,
    // 8/9 = $5130.0/.1; -1 identifica bits ainda não documentados.
    static constexpr int source[4][10] = {
        {0, -1, -1, 0, 1, 2, 3, 4, 5, 6},
        {0, -1, 0, 1, 2, 3, 4, 5, 6, 7},
        {0, 0, 1, 2, 3, 4, 5, 6, 7, 8},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
    unsigned result = 0;
    for (unsigned bit = 0; bit < 10; ++bit)
    {
        const int pin = source[mode][bit];
        if (pin >= 0)
            result |= (((pin < 8 ? data : high) >> (pin < 8 ? pin : pin - 8)) & 1) << bit;
    }
    return result;
}
static unsigned selected(unsigned mode, unsigned page, bool sprite)
{
    static constexpr unsigned a[4][8] = {
        {7, 7, 7, 7, 7, 7, 7, 7}, {3, 3, 3, 3, 7, 7, 7, 7},
        {1, 1, 3, 3, 5, 5, 7, 7}, {0, 1, 2, 3, 4, 5, 6, 7}};
    static constexpr unsigned b[4][8] = {
        {11, 11, 11, 11, 11, 11, 11, 11}, {11, 11, 11, 11, 11, 11, 11, 11},
        {9, 9, 11, 11, 9, 9, 11, 11}, {8, 9, 10, 11, 8, 9, 10, 11}};
    return sprite ? a[mode][page] : b[mode][page];
}
static void mapping(Mapper005 &mapper, unsigned mode, const std::array<unsigned, 12> &banks,
                    unsigned capacity = 1024, unsigned knownBits = 0x3FF)
{
    // Torna os dois conjuntos observáveis para os testes de bancos do lote 1.
    write(mapper, 0x2000, 0x20);
    write(mapper, 0x2001, 0x18);
    const unsigned lowMask = (8u >> mode) - 1;
    for (bool sprite : {false, true})
        for (unsigned page = 0; page < 8; ++page)
            for (unsigned offset : {0u, 1023u})
            {
                const unsigned expected = ((banks[selected(mode, page, sprite)] & ~lowMask) | (page & lowMask)) % capacity;
                const unsigned actual = chr(mapper, page * 1024 + offset, sprite);
                check((actual & 1023) == offset, "CHR offset changed");
                check((((actual / 1024) ^ expected) & (knownBits | lowMask)) == 0, "CHR bank/window mismatch");
            }
}
static void banking()
{
    Mapper005 mapper(32, 128); // 1 MiB de CHR para observar todos os dez bits.
    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned high = 0; high < 4; ++high)
            for (unsigned data = 0; data < 256; ++data)
            {
                write(mapper, 0x5101, mode);
                write(mapper, 0x5130, high);
                std::array<unsigned, 12> banks{};
                for (unsigned reg = 0; reg < 12; ++reg)
                {
                    const uint8_t value = data + reg * 17;
                    write(mapper, 0x5120 + reg, value);
                    banks[reg] = captured(mode, high, value);
                }
                write(mapper, 0x5130, (high + 1) & 3);
                mapping(mapper, mode, banks);
            }
    // Cada registrador deve reter seu próprio $5130, inclusive no conjunto B.
    mapper.reset();
    write(mapper, 0x5101, 3);
    std::array<unsigned, 12> banks{};
    for (unsigned reg = 0; reg < 12; ++reg)
    {
        write(mapper, 0x5130, reg & 3);
        write(mapper, 0x5120 + reg, 7 + reg);
        banks[reg] = ((reg & 3) * 256) + 7 + reg;
    }
    write(mapper, 0x5130, 3);
    mapping(mapper, 3, banks);
    write(mapper, 0x5120, 42);
    banks[0] = 3 * 256 + 42;
    mapping(mapper, 3, banks);

    // Alterar $5101 não reinterpreta nem recaptura o que já foi escrito.
    constexpr unsigned known[4] = {0x3F9, 0x3FD, 0x3FF, 0x3FF};
    for (unsigned writtenMode = 0; writtenMode < 4; ++writtenMode)
    {
        write(mapper, 0x5101, writtenMode);
        write(mapper, 0x5130, 1);
        for (unsigned reg = 0; reg < 12; ++reg)
        {
            write(mapper, 0x5120 + reg, 0xA5 + reg);
            banks[reg] = captured(writtenMode, 1, uint8_t(0xA5 + reg));
        }
        write(mapper, 0x5130, 2);
        for (unsigned mode = 0; mode < 4; ++mode)
        {
            write(mapper, 0x5101, mode);
            mapping(mapper, mode, banks, 1024, known[writtenMode]);
        }
    }
    for (uint8_t size : {0, 1, 4, 32})
    {
        Mapper005 small(32, size);
        for (unsigned mode = 0; mode < 4; ++mode)
        {
            write(small, 0x5101, mode);
            write(small, 0x5130, 3);
            for (unsigned reg = 0; reg < 12; ++reg)
            {
                write(small, 0x5120 + reg, 255 - reg);
                banks[reg] = captured(mode, 3, 255 - reg);
            }
            mapping(small, mode, banks, size ? size * 8 : 8);
            uint32_t mapped = 0;
            check(small.ppuMapWrite(0x1FFF, mapped, 0x5A, 0) == (size == 0), "CHR write protection mismatch");
            if (!size)
            {
                uint8_t data = 0;
                check(mapped == ppuRead(small, 0x1FFF, PpuReadContext::Cpu, data), "CHR RAM read/write mapping differs");
            }
        }
    }
}
static void states()
{
    Mapper005 mapper(32, 128);
    write(mapper, 0x5101, 3);
    std::array<unsigned, 12> banks{};
    for (unsigned reg = 0; reg < 12; ++reg)
    {
        write(mapper, 0x5130, reg & 3);
        write(mapper, 0x5120 + reg, 11 + reg);
        banks[reg] = ((reg & 3) * 256) + 11 + reg;
    }
    write(mapper, 0x5130, 2); // Latch pendente, diferente de vários bancos capturados.
    const auto saved = state(mapper);
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        mapper.reset();
        std::stringstream input(saved);
        mapper.loadState(input);
        check(input.good() && state(mapper) == saved, "Ten-bit CHR state did not round-trip");
        write(mapper, 0x5101, mode);
        mapping(mapper, mode, banks);
    }
    std::stringstream input(saved); mapper.loadState(input);
    write(mapper, 0x5120, 99); banks[0] = 2 * 256 + 99;
    mapping(mapper, 3, banks);
    const auto before = state(mapper);
    for (size_t length : {size_t(0), size_t(13), size_t(14), size_t(29), size_t(37), before.size() - 1})
    {
        std::stringstream truncated(before.substr(0, length));
        mapper.loadState(truncated);
        check(truncated.fail() && state(mapper) == before, "Truncated state changed mapper");
    }
    // Prefixo: oito controles, cinco bancos PRG, doze bancos CHR de dois bytes.
    for (size_t byte : {size_t(1), size_t(14), size_t(36), size_t(37)})
    {
        auto damaged = before; damaged[byte] = 4;
        std::stringstream broken(damaged); mapper.loadState(broken);
        check(broken.fail() && state(mapper) == before, "Invalid CHR state accepted or changed mapper");
    }
}
static void contexts()
{
    Mapper005 mapper(32, 128);
    write(mapper, 0x5101, 3);
    for (unsigned reg = 0; reg < 12; ++reg) write(mapper, 0x5120 + reg, 16 + reg);
    uint8_t data = 0;
    for (uint8_t ctrl : {0, 0x20})
        for (uint8_t mask : {0, 8, 0x10, 0x18})
            for (bool lastB : {false, true})
            {
                write(mapper, 0x2000, ctrl); write(mapper, 0x2001, mask);
                write(mapper, lastB ? 0x5128 : 0x5120, lastB ? 24 : 16);
                for (auto context : {PpuReadContext::Cpu, PpuReadContext::Background, PpuReadContext::Sprite})
                    for (unsigned page = 0; page < 8; ++page)
                    {
                        const bool setA = !ctrl || ((context == PpuReadContext::Cpu || !mask)
                            ? !lastB : context == PpuReadContext::Sprite);
                        const unsigned bank = setA ? 16 + page : 24 + (page & 3);
                        check(ppuRead(mapper, page * 1024 + 321, context, data) == bank * 1024 + 321,
                              "Incorrect CHR set for PPU context/control/mask");
                    }
            }
    write(mapper, 0x2000, 0x20); write(mapper, 0x2001, 0x18);
    write(mapper, 0x5120, 16); // A é o último escrito, mas background deve usar B.
    for (uint16_t mirror : {0x2008, 0x2010, 0x3FF8, 0x2009, 0x2011, 0x3FF9})
    {
        write(mapper, mirror, 0);
        check(chr(mapper, 0, false) == 24 * 1024 && chr(mapper, 0, true) == 16 * 1024,
              "MMC5 observed a mirror of PPUCTRL/PPUMASK");
    }
    for (uint16_t control : {0x2000, 0x2001})
    {
        uint32_t mapped = 0;
        check(!mapper.cpuMapWrite(control, mapped, control == 0x2000 ? 0x20 : 0x18, 0),
              "Observing a PPU write consumed it");
    }
    write(mapper, 0x512A, 26); // Até registradores que não afetam a página lida mudam a seleção.
    check(ppuRead(mapper, 0, PpuReadContext::Cpu, data) == 24 * 1024, "Last B register not tracked");
    ppuRead(mapper, 0x2000, PpuReadContext::Cpu, data);
    ppuRead(mapper, 0x23C0, PpuReadContext::Cpu, data);
    check(ppuRead(mapper, 0, PpuReadContext::Cpu, data) == 24 * 1024, "Nametable read inferred a fetch context");
    write(mapper, 0x5124, 20);
    check(ppuRead(mapper, 0, PpuReadContext::Cpu, data) == 16 * 1024, "Last A register not tracked");
    const auto saved = state(mapper);
    mapper.reset(); std::stringstream restore(saved); mapper.loadState(restore);
    check(restore.good() && ppuRead(mapper, 0, PpuReadContext::Cpu, data) == 16 * 1024 &&
          chr(mapper, 0, false) == 24 * 1024 && chr(mapper, 0, true) == 16 * 1024,
          "CHR context controls did not survive restore");
    write(mapper, 0x2001, 0);
    const auto disabled = state(mapper);
    mapper.reset(); std::stringstream reload(disabled); mapper.loadState(reload);
    check(reload.good() && chr(mapper, 0, false) == 16 * 1024, "Disabled substitutions not restored");
    for (size_t byte : {saved.size() - 14, saved.size() - 13, saved.size() - 12, saved.size() - 11})
    {
        const auto before = state(mapper);
        auto damaged = saved; damaged[byte] = 2;
        std::stringstream bad(damaged); mapper.loadState(bad);
        check(bad.fail() && state(mapper) == before, "Invalid CHR selector state accepted");
    }
}
static void extendedAttributes()
{
    Mapper005 mapper(32, 128);
    uint32_t mapped = 0;
    uint8_t data = 0;
    write(mapper, 0x5106, 0x5A); write(mapper, 0x5107, 2);
    for (unsigned source = 0; source < 4; ++source)
    {
        write(mapper, 0x5105, source * 0x55);
        for (unsigned nt = 0; nt < 4; ++nt)
        {
            const uint16_t tileAddress = 0x2000 + nt * 1024 + 0x22;
            if (source < 2) mapper.ppuMapWrite(tileAddress, mapped, 0x2A, 0);
            for (unsigned mode = 0; mode < 4; ++mode)
                for (uint8_t ctrl : {0, 0x20})
                    for (uint8_t mask : {8, 0x10, 0x18})
                        for (unsigned palette = 0; palette < 4; ++palette)
                            for (unsigned high = 0; high < 4; ++high)
                            {
                                const uint8_t attribute = (palette << 6) | 0x21;
                                write(mapper, 0x5104, 2);
                                write(mapper, 0x5C22, attribute);
                                write(mapper, 0x5C43, attribute ^ 0xFF);
                                write(mapper, 0x5FC0, 0x04); // Não é o atributo do tile.
                                write(mapper, 0x5104, 1);
                                write(mapper, 0x5101, mode);
                                write(mapper, 0x2000, ctrl); write(mapper, 0x2001, mask);
                                ppuRead(mapper, tileAddress, PpuReadContext::Background, data);
                                check(data == (source < 2 ? 0x2A : source == 2 ? attribute : 0x5A), "Extended mode changed nametable tile source");
                                // Acessos de CPU, sprites e visualizador não podem trocar o tile latched.
                                ppuRead(mapper, 0x2043, PpuReadContext::Cpu, data);
                                ppuRead(mapper, 0x2043, PpuReadContext::Sprite, data);
                                const auto beforePeek = state(mapper);
                                mapper.ppuMapPeek(0x2043, mapped, data, 0);
                                mapper.ppuMapPeek(0x1FFF, mapped, data, 0);
                                check(state(mapper) == beforePeek, "Peek changed MMC5 tile state");
                                ppuRead(mapper, 0x23C0 + nt * 1024, PpuReadContext::Background, data);
                                check(data == palette * 0x55, "Extended palette did not follow nametable tile");
                                // $5130 deve valer imediatamente, sem outra leitura de tile.
                                write(mapper, 0x5130, high);
                                for (uint16_t address : {0, 0x3FF, 0x400, 0xFFF, 0x1000, 0x1FFF})
                                    check(chr(mapper, address, false) == ((high * 64 + 0x21) * 4096 + (address & 0xFFF)),
                                          "Extended CHR bank is not a live 4 KiB selection");
                                // Desligar substituições restaura atributos normais, inclusive fill color.
                                write(mapper, 0x2001, 0);
                                ppuRead(mapper, 0x23C0 + nt * 1024, PpuReadContext::Background, data);
                                if (source == 3) check(data == 0xAA, "Fill color not restored when substitutions disabled");
                                if (source == 2) check(data == 4, "Raw ExRAM attribute changed with substitutions disabled");
                            }
        }
    }
    write(mapper, 0x5105, 0); write(mapper, 0x2001, 0x18);
    write(mapper, 0x5104, 2); write(mapper, 0x5C22, 0xC1); write(mapper, 0x5104, 1);
    ppuRead(mapper, 0x2022, PpuReadContext::Background, data);
    write(mapper, 0x5130, 1);
    const auto saved = state(mapper);
    mapper.reset(); std::stringstream restore(saved); mapper.loadState(restore);
    ppuRead(mapper, 0x23C0, PpuReadContext::Background, data);
    check(restore.good() && data == 0xFF && chr(mapper, 0x1FFF, false) == 65 * 4096 + 0xFFF,
          "Extended tile latch not restored between tile and pattern fetches");
    for (uint8_t size : {0, 1, 4})
    {
        Mapper005 small(32, size);
        write(small, 0x2001, 8); write(small, 0x5104, 2); write(small, 0x5C00, 0xFF);
        write(small, 0x5104, 1); write(small, 0x5130, 3);
        ppuRead(small, 0x2000, PpuReadContext::Background, data);
        check(chr(small, 0x1FFF, false) == (255 * 4096 + 0xFFF) % (size ? size * 8192 : 8192),
              "Extended CHR capacity wrapping mismatch");
    }
}
static uint8_t irqStatus(Mapper005 &mapper, bool peek = true)
{
    uint32_t mapped = 0; uint8_t data = 0;
    check(peek ? mapper.cpuMapPeek(0x5204, mapped, data) : mapper.cpuMapRead(0x5204, mapped, data), "IRQ status not mapped");
    return data & 0xC0;
}
static void detectedLine(Mapper005 &mapper)
{
    mapper.onPpuRead(0); // Interrompe a sequência da linha anterior.
    mapper.onPpuRead(0x2000); mapper.onPpuRead(0x2000); mapper.onPpuRead(0x2000);
    mapper.onPpuRead(0x23C0);
}
static void pendingIrq(Mapper005 &mapper)
{
    mapper.reset(); write(mapper, 0x5203, 1); write(mapper, 0x5204, 0x80);
    detectedLine(mapper); detectedLine(mapper);
    check(mapper.getIrqFlag() && irqStatus(mapper) == 0xC0, "IRQ setup failed");
}
static void scanlineIrqs()
{
    Mapper005 mapper(32, 128);
    write(mapper, 0x5203, 1);
    for (unsigned i = 0; i < 3; ++i) mapper.onPpuRead(0x2000);
    check(irqStatus(mapper) == 0, "Scanline detected before fourth read");
    mapper.onPpuRead(0x23C0);
    check(irqStatus(mapper) == 0x40 && !mapper.getIrqFlag(), "First detected scanline must be zero");
    detectedLine(mapper);
    check(irqStatus(mapper) == 0xC0 && !mapper.getIrqFlag(), "Disabled IRQ did not retain pending event");
    write(mapper, 0x5204, 0x80);
    check(mapper.getIrqFlag(), "Enabling pending IRQ did not assert line");
    write(mapper, 0x5204, 0);
    check(!mapper.getIrqFlag() && irqStatus(mapper) == 0xC0, "Disabling IRQ acknowledged pending event");
    write(mapper, 0x5204, 0xFF);
    const auto beforePeek = state(mapper);
    check(irqStatus(mapper) == 0xC0 && state(mapper) == beforePeek, "IRQ peek acknowledged event");
    mapper.clearIrqFlag();
    check(mapper.getIrqFlag(), "CPU acceptance acknowledged MMC5 IRQ");
    write(mapper, 0x5203, 0);
    check(mapper.getIrqFlag(), "Writing target zero acknowledged existing IRQ");
    check(irqStatus(mapper, false) == 0xC0 && irqStatus(mapper) == 0x40 && !mapper.getIrqFlag(), "IRQ status acknowledge changed frame state");
    write(mapper, 0x5203, 2); detectedLine(mapper);
    check(mapper.getIrqFlag(), "IRQ acknowledge reset scanline counter");

    // Três M2 vazios encerram in-frame, sem reconhecer um evento pendente.
    mapper.tick(); // Consome a atividade do último grupo de leituras.
    mapper.tick(); mapper.tick();
    check(irqStatus(mapper) == 0xC0, "Frame timeout occurred before three empty M2 cycles");
    mapper.tick();
    check(irqStatus(mapper) == 0x80 && mapper.getIrqFlag(), "Frame timeout cleared pending IRQ or failed to end frame");
    detectedLine(mapper);
    check(irqStatus(mapper) == 0x40 && !mapper.getIrqFlag(), "New scanline zero did not acknowledge old IRQ");
    for (unsigned i = 0; i < 300; ++i)
    {
        mapper.onPpuRead(0x1000); mapper.tick(); mapper.tick();
        check(irqStatus(mapper) == 0x40, "Continuing pattern reads lost in-frame state");
    }
    for (uint16_t address : {0x0000, 0x1FFF, 0x3000, 0x3EFF})
    {
        mapper.reset();
        for (unsigned i = 0; i < 4; ++i) mapper.onPpuRead(address);
        check(irqStatus(mapper) == 0, "Non-$2xxx reads clocked scanline");
    }
    mapper.reset();
    for (uint16_t address : {0x2000, 0x2001, 0x2000, 0x23C0, 0x2000, 0, 0x2000, 0x2000}) mapper.onPpuRead(address);
    check(irqStatus(mapper) == 0, "Interrupted/mismatched reads clocked scanline");
    for (unsigned i = 0; i < 4; ++i) mapper.tick();
    mapper.onPpuRead(0x2000); mapper.onPpuRead(0x23C0);
    check(irqStatus(mapper) == 0, "Idle timeout did not discard partial match");
    mapper.reset();
    for (unsigned i = 0; i < 3; ++i) mapper.onPpuRead(0x2FFF);
    mapper.onPpuRead(0);
    check(irqStatus(mapper) == 0x40, "Fourth read must detect scanline regardless of its address");

    // O callback antigo, alterações dos pinos e consultas não representam /RD.
    mapper.reset(); uint32_t mapped = 0; uint8_t data = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        mapper.onPpuScanlineStart(); mapper.onPpuAddress(0x2000, i);
        mapper.ppuMapPeek(0x2000, mapped, data, 0);
        mapper.ppuMapWrite(0x2000, mapped, 0, 0);
    }
    check(irqStatus(mapper) == 0, "Non-read operation clocked scanline");

    for (unsigned target : {0, 1, 119, 239, 240, 255})
    {
        mapper.reset(); write(mapper, 0x5203, target); write(mapper, 0x5204, 0x80);
        for (unsigned line = 0; line < 241; ++line)
        {
            detectedLine(mapper);
            const bool pending = target && target < 240 && line >= target && line < 240;
            check(mapper.getIrqFlag() == pending, "Scanline target/241st-line reset mismatch");
        }
        check(irqStatus(mapper) == 0, "241st detected scanline did not reset frame/IRQ");
    }
    for (unsigned resetKind = 0; resetKind < 6; ++resetKind)
    {
        pendingIrq(mapper);
        if (resetKind < 2) mapper.cpuMapRead(resetKind ? 0xFFFB : 0xFFFA, mapped, data);
        else if (resetKind == 2) mapper.onCpuRead(0x2002, 0x80);
        else if (resetKind == 3) write(mapper, 0x4014, 0);
        else
        {
            write(mapper, 0x2001, 8); // Primeira habilitação também reseta.
            if (resetKind == 5) { detectedLine(mapper); detectedLine(mapper); write(mapper, 0x2001, 0); write(mapper, 0x2001, 0x10); }
        }
        check(irqStatus(mapper) == 0 && !mapper.getIrqFlag(), "Observed reset did not clear frame/IRQ");
        detectedLine(mapper); detectedLine(mapper);
        check(mapper.getIrqFlag(), "Observed reset changed IRQ enable/target");
    }
    pendingIrq(mapper);
    mapper.onCpuRead(0x2002, 0); mapper.onCpuRead(0x200A, 0x80);
    write(mapper, 0x2009, 0); write(mapper, 0x4015, 0);
    mapper.cpuMapPeek(0xFFFA, mapped, data); mapper.cpuMapPeek(0xFFFB, mapped, data);
    check(mapper.getIrqFlag(), "Mirror/non-vblank/peek incorrectly reset IRQ");
    write(mapper, 0x2001, 8); detectedLine(mapper); detectedLine(mapper);
    write(mapper, 0x2001, 0x10);
    check(mapper.getIrqFlag(), "Nonzero-to-nonzero PPUMASK transition reset IRQ");
    write(mapper, 0x2001, 0);
    check(irqStatus(mapper) == 0x80 && mapper.getIrqFlag(), "Disabling substitutions acknowledged IRQ");

    // Estado no meio da sequência e no meio do intervalo de atividade M2.
    mapper.reset(); write(mapper, 0x5203, 1); write(mapper, 0x5204, 0x80);
    detectedLine(mapper); mapper.onPpuRead(0); mapper.tick();
    for (unsigned matches = 1; matches <= 3; ++matches)
    {
        mapper.onPpuRead(0x2000);
        const auto snapshot = state(mapper);
        mapper.reset(); std::stringstream restore(snapshot); mapper.loadState(restore);
        check(restore.good() && state(mapper) == snapshot, "Partial scanline detector state not restored");
    }
    mapper.onPpuRead(0x23C0);
    check(mapper.getIrqFlag(), "Restored armed detector did not fire on fourth read");
    mapper.tick(); mapper.tick(); mapper.tick();
    const auto timeout = state(mapper);
    mapper.reset(); std::stringstream restore(timeout); mapper.loadState(restore); mapper.tick();
    check(restore.good() && irqStatus(mapper) == 0x80, "Restored M2 inactivity timeout shifted");
    const auto before = state(mapper);
    for (size_t byte : {size_t(41), size_t(42), size_t(43), size_t(44), before.size() - 19, before.size() - 18, before.size() - 11})
    {
        auto damaged = before; damaged[byte] = byte == 44 ? char(240) : char(4);
        std::stringstream invalid(damaged); mapper.loadState(invalid);
        check(invalid.fail() && state(mapper) == before, "Invalid IRQ detector state changed mapper");
    }
}
static uint8_t cpuByte(Mapper005 &mapper, uint16_t address, uint8_t openBus = 0, bool peek = false)
{
    uint32_t mapped = 0; uint8_t data = openBus;
    check(peek ? mapper.cpuMapPeek(address, mapped, data) : mapper.cpuMapRead(address, mapped, data), "CPU read not mapped");
    check(mapped == 0xFFFFFFFF, "ExRAM CPU read escaped to ROM");
    return data;
}
static void initialMemoryModes()
{
    for (auto revision : {Mapper005::Revision::MMC5, Mapper005::Revision::MMC5A})
    {
        Mapper005::Configuration config; config.revision = revision;
        Mapper005 mapper(32, 128, config);
        const auto verify = [&](uint8_t retained)
        {
            // Antes de qualquer escrita em $5104, ExRAM deve ser legível e protegida.
            check(cpuByte(mapper, 0x5C00, 0xA5) == retained &&
                  cpuByte(mapper, 0x5C00, 0xA5, true) == retained, "Initial ExRAM mode is not read-only CPU RAM");
            write(mapper, 0x5C00, 0x11);
            write(mapper, 0x5105, 0xAA);
            for (uint16_t address : {0x2000, 0x2400, 0x2800, 0x2C00})
            {
                uint32_t mapped = 0; uint8_t data = 0xFF;
                mapper.ppuMapWrite(address, mapped, 0x96, 0);
                ppuRead(mapper, address, PpuReadContext::Cpu, data);
                check(data == 0, "Initial ExRAM mode exposed a nametable");
            }
            check(cpuByte(mapper, 0x5C00) == retained, "Initial ExRAM CPU/PPU protection failed");
            // O desbloqueio de WRAM exige ambas as escritas, em qualquer ordem.
            write(mapper, 0x6000, 0x11);
            check(cpuByte(mapper, 0x6000) == retained, "Initial WRAM protection failed");
            write(mapper, 0x5102, 2); write(mapper, 0x6000, 0x22);
            check(cpuByte(mapper, 0x6000) == retained, "One protection register unlocked WRAM");
            write(mapper, 0x5103, 1); write(mapper, 0x6000, 0x33);
            check(cpuByte(mapper, 0x6000) == 0x33, "Explicit WRAM unlock failed");
        };
        verify(0);
        write(mapper, 0x5104, 2); write(mapper, 0x5C00, 0x5A); write(mapper, 0x6000, 0x5A);
        mapper.reset(); verify(0x5A);
        mapper.reset();
        write(mapper, 0x5103, 1); write(mapper, 0x6000, 0x44);
        check(cpuByte(mapper, 0x6000) == 0x33, "Reverse-order partial unlock changed WRAM");
        write(mapper, 0x5102, 2); write(mapper, 0x6000, 0x44);
        check(cpuByte(mapper, 0x6000) == 0x44, "Reverse-order full unlock failed");
    }
}
static void exRamPermissions()
{
    Mapper005 mapper(32, 128);
    for (unsigned mode = 0; mode < 4; ++mode)
        for (bool rendering : {false, true})
        {
            mapper.reset(); write(mapper, 0x5104, 2);
            for (unsigned offset = 0; offset < 1024; ++offset) write(mapper, 0x5C00 + offset, 0x5A);
            if (rendering) detectedLine(mapper);
            write(mapper, 0x5104, mode);
            const auto before = state(mapper);
            for (unsigned offset = 0; offset < 1024; ++offset)
            {
                const uint8_t bus = offset ^ 0x96;
                check(cpuByte(mapper, 0x5C00 + offset, bus) == (mode < 2 ? bus : 0x5A), "ExRAM CPU read permission mismatch");
                check(cpuByte(mapper, 0x5C00 + offset, bus, true) == (mode < 2 ? bus : 0x5A), "ExRAM peek bypassed CPU permissions");
            }
            check(state(mapper) == before, "ExRAM reads/peeks changed state");
            for (unsigned offset = 0; offset < 1024; ++offset) write(mapper, 0x5C00 + offset, offset ^ 0xA5);
            const bool writable = mode == 2 || (mode < 2 && rendering);
            write(mapper, 0x5104, 2);
            for (unsigned offset = 0; offset < 1024; ++offset)
                check(cpuByte(mapper, 0x5C00 + offset) == (writable ? uint8_t(offset ^ 0xA5) : 0x5A), "ExRAM CPU write permission mismatch");
        }
    // O acesso de nametable deve desaparecer em ambos os modos de RAM da CPU.
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        mapper.reset(); write(mapper, 0x5104, 2); write(mapper, 0x5105, 0xAA);
        for (unsigned offset = 0; offset < 1024; ++offset) write(mapper, 0x5C00 + offset, 0x5A);
        write(mapper, 0x5104, mode);
        for (unsigned nt = 0; nt < 4; ++nt)
            for (unsigned offset = 0; offset < 1024; ++offset)
            {
                uint32_t mapped = 0; uint8_t data = 0;
                const uint16_t address = 0x2000 + nt * 1024 + offset;
                check(mapper.ppuMapWrite(address, mapped, offset ^ nt ^ 0xA5, 0) && mapped == 0xFFFFFFFF,
                      "ExRAM nametable write not handled internally");
                ppuRead(mapper, address, PpuReadContext::Cpu, data);
                check(data == (mode < 2 ? uint8_t(offset ^ nt ^ 0xA5) : 0), "ExRAM nametable access permission mismatch");
                if (address < 0x2F00)
                {
                    uint8_t mirror = 0; ppuRead(mapper, address + 0x1000, PpuReadContext::Cpu, mirror);
                    check(mirror == data, "ExRAM $3000 mirroring mismatch");
                }
            }
        write(mapper, 0x5104, 2);
        for (unsigned offset = 0; offset < 1024; ++offset)
            check(cpuByte(mapper, 0x5C00 + offset) == (mode < 2 ? uint8_t(offset ^ 3 ^ 0xA5) : 0x5A), "Blocked nametable write changed ExRAM");
    }
    mapper.reset(); write(mapper, 0x5104, 2); write(mapper, 0x5C00, 0x5A);
    write(mapper, 0x5104, 0); detectedLine(mapper);
    const auto active = state(mapper);
    mapper.reset(); std::stringstream restore(active); mapper.loadState(restore);
    check(restore.good(), "Active ExRAM state rejected");
    write(mapper, 0x5C00, 0xA5);
    mapper.tick(); mapper.tick(); mapper.tick(); mapper.tick();
    write(mapper, 0x5C00, 0x11); // Ignorada depois do timeout de atividade.
    write(mapper, 0x5104, 2);
    check(cpuByte(mapper, 0x5C00) == 0xA5, "ExRAM write gating lost restored frame/idle state");
}
static void strictStatesAndSplit()
{
    Mapper005 mapper(32, 128);
    write(mapper, 0x5104, 2);
    for (unsigned offset = 0; offset < 1024; ++offset) write(mapper, 0x5C00 + offset, offset ^ (offset >> 3));
    write(mapper, 0x2001, 0x18); write(mapper, 0x5105, 0);
    uint8_t data = 0;
    for (unsigned scroll : {0, 7, 8, 239, 240, 255})
        for (unsigned line : {0, 1, 8, 239})
            for (unsigned threshold : {0, 1, 8, 31})
                for (bool right : {false, true})
                    for (unsigned column = 0; column < 34; ++column)
                    {
                        write(mapper, 0x5201, scroll);
                        mapper.onPpuBackgroundTile(-1, 0);
                        write(mapper, 0x5200, 0x80 | (right ? 0x40 : 0) | threshold);
                        write(mapper, 0x5202, 65); write(mapper, 0x5130, 3);
                        write(mapper, 0x5104, 0);
                        mapper.onPpuBackgroundTile(line, column);
                        unsigned y = scroll;
                        for (unsigned i = 0; i < line; ++i) y = y == 239 || y == 255 ? 0 : y + 1;
                        const unsigned offset = (y / 8) * 32 + (column & 31);
                        const bool split = right ? column >= threshold : column < threshold;
                        ppuRead(mapper, 0x2022, PpuReadContext::Background, data);
                        check(data == (split ? uint8_t(offset ^ (offset >> 3)) : 0), "Split nametable tile/threshold/vertical scroll mismatch");
                        ppuRead(mapper, 0x23C0, PpuReadContext::Background, data);
                        const unsigned attr = 0x3C0 + (y / 32) * 8 + ((column & 31) / 4);
                        const unsigned shift = ((y / 8) & 2) * 2 + (column & 2);
                        const unsigned palette = ((attr ^ (attr >> 3)) >> shift) & 3;
                        check(data == (split ? palette * 0x55 : 0), "Split palette quadrant mismatch");
                        for (uint16_t address : {0x0013, 0x1007, 0x1FFF})
                            check(chr(mapper, address, false) == (split ? 65 * 4096u + (address & 0xFFF) : address),
                                  "Split bank/mirroring/CL fine Y mismatch");
                        check(chr(mapper, 7, true) == 7, "Split changed sprite pattern mapping");
                        uint32_t mapped = 0;
                        mapper.ppuMapPeek(7, mapped, data, 0);
                        check(mapped == 7, "Split changed PPUDATA/debugger CHR window");
                    }
    for (unsigned mode = 0; mode < 4; ++mode)
        for (unsigned mask : {0, 8, 0x10, 0x18})
            for (bool enabled : {false, true})
            {
                write(mapper, 0x5104, mode); write(mapper, 0x2001, mask);
                write(mapper, 0x5200, enabled ? 0xC0 : 0); write(mapper, 0x5202, 1);
                mapper.onPpuBackgroundTile(0, 5);
                // Modos 0/1 substituem CHR só com split e PPUMASK habilitados.
                // No modo 1 fora do split, ExGrafix ainda tem precedência sobre CHR normal.
                if (mode != 1 || !mask || enabled)
                    check(chr(mapper, 0, false) == ((enabled && mask && mode <= 1) ? 4096u : 0u), "Split enable/ExRAM/mask gate mismatch");
            }
    write(mapper, 0x5104, 1); write(mapper, 0x2001, 8);
    write(mapper, 0x5200, 0xC8); write(mapper, 0x5201, 0); write(mapper, 0x5202, 1);
    mapper.onPpuBackgroundTile(-1, 0); mapper.onPpuBackgroundTile(0, 7);
    ppuRead(mapper, 0x2022, PpuReadContext::Background, data);
    const unsigned ext = uint8_t(0x22 ^ (0x22 >> 3));
    check(chr(mapper, 0, false) == ((3 * 64 + (ext & 63)) * 4096u), "Non-split region lost extended attributes");
    mapper.onPpuBackgroundTile(0, 8);
    check(chr(mapper, 0, false) == 4096, "Split did not override extended attributes");
    const auto saved = state(mapper);
    mapper.reset(); std::stringstream restore(saved); mapper.loadState(restore);
    check(restore.good() && state(mapper) == saved && chr(mapper, 0, false) == 4096, "Active split did not round-trip");
    // Escritas de scroll são carregadas no início do quadro, não no meio da linha.
    write(mapper, 0x5201, 8); mapper.onPpuBackgroundTile(0, 9);
    ppuRead(mapper, 0x2000, PpuReadContext::Background, data);
    check(data == uint8_t(9 ^ (9 >> 3)), "Split scroll write recaptured current line");
    mapper.onPpuBackgroundTile(-1, 0); mapper.onPpuBackgroundTile(0, 9);
    ppuRead(mapper, 0x2000, PpuReadContext::Background, data);
    check(data == uint8_t(41 ^ (41 >> 3)), "Split scroll not loaded for next frame");

    const auto before = state(mapper);
    const auto reject = [&](std::string bytes)
    {
        std::stringstream input(bytes); mapper.loadState(input);
        check(input.fail() && state(mapper) == before, "Invalid semantic state accepted or partially applied");
    };
    for (size_t field : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4), size_t(7), size_t(37)})
        for (unsigned value : {4, 0x7F, 0xFF})
        {
            auto bytes = before; bytes[field] = value; reject(bytes);
        }
    for (size_t field : {size_t(41), size_t(42), size_t(43)})
    {
        auto bytes = before; bytes[field] = 2; reject(bytes);
    }
    for (const auto item : {std::pair<size_t, uint8_t>{before.size() - 10, 0x20},
            {before.size() - 7, 240}, {before.size() - 4, 34}, {before.size() - 3, 2},
            {before.size() - 1, 4}, {before.size() - 16, 4}})
    {
        auto bytes = before; bytes[item.first] = item.second; reject(bytes);
    }
    auto incoherent = before; incoherent[incoherent.size() - 2] ^= 1; reject(incoherent);
    incoherent = before; incoherent[incoherent.size() - 21] = char(0xFF); incoherent[incoherent.size() - 20] = char(0xFF);
    incoherent[incoherent.size() - 19] = 1; reject(incoherent); // Coincidência exige endereço $2xxx.
    for (size_t length = 0; length < 68; ++length) reject(before.substr(0, length));
    for (size_t length = before.size() - 23; length < before.size(); ++length) reject(before.substr(0, length));
    for (size_t length : {size_t(100), size_t(65536), before.size() / 2, before.size() - 1030}) reject(before.substr(0, length));
    for (uint8_t size : {0, 1, 4})
    {
        Mapper005 small(32, size);
        write(small, 0x5104, 2); write(small, 0x5C00, 0x72); write(small, 0x5FC0, 0xAA);
        write(small, 0x5104, 0); write(small, 0x2001, 8);
        write(small, 0x5200, 0xC0); write(small, 0x5202, 255);
        small.onPpuBackgroundTile(0, 0);
        check(chr(small, 0x1FFF, false) == (255 * 4096 + 0xFFF) % (size ? size * 8192 : 8192), "Split CHR capacity wrapping mismatch");
        ppuRead(small, 0x23C0, PpuReadContext::BackgroundTile, data);
        check(data == 0x72, "Nametable tile fetch was mistaken for attribute by its address");
        write(small, 0x5200, 0x80); // Split à esquerda de zero: nenhuma coluna.
        small.onPpuBackgroundTile(0, 0); write(small, 0x5104, 1);
        ppuRead(small, 0x23C0, PpuReadContext::BackgroundTile, data);
        const auto unusualTile = state(small);
        std::stringstream input(unusualTile); small.loadState(input);
        check(input.good(), "Legitimate attribute-region tile address rejected by state validation");
    }
}
static void wramAndRegisters()
{
    using Config = Mapper005::Configuration;
    for (const Config config : {Config{0, 0}, Config{8192, 0}, Config{16384, 0}, Config{32768, 0},
            Config{65536, 0}, Config{131072, 0}, Config{8192, 8192}, Config{32768, 8192}, Config{8192, 32768}})
    {
        Mapper005 mapper(32, 128, config);
        write(mapper, 0x5102, 2); write(mapper, 0x5103, 1);
        const unsigned total = config.prgRamSize + config.prgNvRamSize;
        unsigned first = total, second = 0;
        if (config.prgRamSize && config.prgNvRamSize) { first = config.prgNvRamSize; second = config.prgRamSize; }
        else if (total == 16384 || total == 65536) first = second = total / 2;
        const auto physical = [&](unsigned bank) -> int
        {
            if (!total) return -1;
            if (total == 131072) return bank & 15;
            if ((bank & 4) && !second) return -1;
            return ((bank & 4) ? first / 8192 : 0) + (bank & 3) % (((bank & 4) ? second : first) / 8192);
        };
        // Cada banco físico recebe uma assinatura pela janela baixa.
        for (unsigned bank = 0; bank < 16; ++bank)
        {
            write(mapper, 0x5113, bank);
            const int page = physical(bank);
            if (page >= 0) { write(mapper, 0x6000, 0x40 + page); write(mapper, 0x7FFF, 0x60 + page); }
        }
        for (unsigned mode = 0; mode < 4; ++mode)
            for (unsigned value = 0; value < 256; ++value)
            {
                write(mapper, 0x5100, mode);
                for (unsigned reg = 0; reg < 5; ++reg) write(mapper, 0x5113 + reg, value);
                check(cpuByte(mapper, 0x6000, 0x96) == (physical(value) < 0 ? 0x96 : 0x40 + physical(value)), "WRAM low-bank mapping mismatch");
                for (unsigned slot = 0; slot < 4; ++slot)
                {
                    unsigned bank = value & 0x7F;
                    bool ram = slot < 3 && !(value & 128);
                    if (mode == 0) { bank = (value & 0x7C) + slot; ram = false; }
                    if (mode == 1) { bank = (value & 0x7E) + (slot & 1); ram = slot < 2 && !(value & 128); }
                    if (mode == 2 && slot < 2) bank = (value & 0x7E) + slot;
                    for (unsigned offset : {0, 8191})
                    {
                        uint32_t mapped = 0; uint8_t data = 0x96;
                        const uint16_t address = 0x8000 + slot * 8192 + offset;
                        check(mapper.cpuMapRead(address, mapped, data), "PRG window not mapped");
                        if (!ram) check(mapped == (bank % 64) * 8192 + offset, "PRG ROM mapping regressed");
                        else check(mapped == 0xFFFFFFFF && data == (physical(bank) < 0 ? 0x96 : (offset ? 0x60 : 0x40) + physical(bank)), "PRG RAM window/chip selection mismatch");
                    }
                }
            }
        write(mapper, 0x5100, 3);
        for (unsigned bank = 0; bank < 16; ++bank)
        {
            const int page = physical(bank);
            write(mapper, 0x5114, bank); write(mapper, 0x8001, 0xA0 + bank);
            write(mapper, 0x5113, bank);
            check(cpuByte(mapper, 0x6001, 0x96) == (page < 0 ? 0x96 : 0xA0 + bank), "WRAM high write not visible through low window");
            write(mapper, 0x5103, 0); write(mapper, 0x8001, 0);
            check(cpuByte(mapper, 0x6001, 0x96) == (page < 0 ? 0x96 : 0xA0 + bank), "Protected WRAM changed");
            write(mapper, 0x5103, 1);
        }
        const auto snapshot = state(mapper);
        mapper.reset(); std::stringstream restore(snapshot); mapper.loadState(restore);
        check(restore.good() && state(mapper) == snapshot, "Sized WRAM did not round-trip");
        check(snapshot.size() == 3180 + total, "WRAM state payload not sized to installed memory");
        Config other = config; other.prgRamSize = total == 8192 ? 0 : 8192; other.prgNvRamSize = 0;
        Mapper005 incompatible(32, 128, other); const auto untouched = state(incompatible);
        std::stringstream wrong(snapshot); incompatible.loadState(wrong);
        check(wrong.fail() && state(incompatible) == untouched, "State with different WRAM capacity accepted");
    }
    Mapper005 mapper(32, 128);
    for (uint16_t reg : {0x5100, 0x5101, 0x5102, 0x5103, 0x5104, 0x5105, 0x5106, 0x5107,
            0x5113, 0x5114, 0x5115, 0x5116, 0x5117, 0x5120, 0x5127, 0x5128, 0x512B, 0x5130,
            0x5200, 0x5201, 0x5202, 0x5203, 0x5204, 0x5205, 0x5206})
    {
        uint32_t mapped = 0;
        check(mapper.cpuMapWrite(reg, mapped, 0xFF, 0) && mapped == 0xFFFFFFFF, "MMC5 register write not handled internally");
    }
    const auto before = state(mapper);
    for (uint16_t hole : {0x5108, 0x5112, 0x5118, 0x511F, 0x512C, 0x512F, 0x5131, 0x5207})
    {
        uint32_t mapped = 0;
        check(!mapper.cpuMapWrite(hole, mapped, 0xA5, 0) && state(mapper) == before, "Register hole consumed write/changed state");
    }
    std::array<uint8_t, 512> trainer{};
    for (unsigned i = 0; i < trainer.size(); ++i) trainer[i] = i ^ 0xA5;
    mapper.reset(); mapper.loadTrainer(trainer.data());
    for (unsigned i = 0; i < trainer.size(); ++i) check(cpuByte(mapper, 0x7000 + i) == trainer[i], "Trainer not installed at $7000");
    write(mapper, 0x7000, 0);
    check(cpuByte(mapper, 0x7000) == trainer[0], "Trainer installation unlocked PRG RAM");
}
static void mmc5a()
{
    Mapper005::Configuration config;
    config.revision = Mapper005::Revision::MMC5A;
    Mapper005 mapper(32, 128, config), original(32, 128);
    uint32_t mapped = 0; uint8_t data = 0xA5;
    for (uint16_t addr : {0x5207, 0x5208, 0x5209, 0x520A})
        check(!original.cpuMapRead(addr, mapped, data) && !original.cpuMapWrite(addr, mapped, 1, 0),
              "MMC5 decoded MMC5A-only registers");
    for (uint16_t addr : {0x5207, 0x520A})
        check(!mapper.cpuMapRead(addr, mapped, data), "Write-only MMC5A register drove a read");

    for (unsigned count : {0, 1, 2, 255, 256, 65535})
    {
        mapper.reset(); write(mapper, 0x520A, count >> 8);
        for (unsigned i = 0; i < 3; ++i) mapper.tick();
        check(!mapper.getIrqFlag(), "Writing timer high byte started timer");
        write(mapper, 0x5209, count & 255);
        for (unsigned i = 1; i < count; ++i)
        {
            mapper.tick();
            check(!mapper.getIrqFlag() && cpuByte(mapper, 0x5209, 0, true) == 0, "Timer expired early");
        }
        mapper.tick();
        check(mapper.getIrqFlag() == (count != 0), "Timer did not expire on the 1-to-0 transition");
        check(cpuByte(mapper, 0x5209, 0, true) == (count ? 0x80 : 0), "Timer peek status mismatch");
        mapper.clearIrqFlag(); write(mapper, 0x5204, 0); cpuByte(mapper, 0x5204);
        check(mapper.getIrqFlag() == (count != 0), "Scanline control or CPU acceptance acknowledged timer");
        cpuByte(mapper, 0x5209);
        check(!mapper.getIrqFlag(), "Timer read did not acknowledge timer");
        for (unsigned i = 0; i < 3; ++i) mapper.tick();
        check(!mapper.getIrqFlag(), "Timer free-ran after expiry or zero start");
    }
    // Escritas alteram a parte correspondente do contador atual, não uma recarga futura.
    mapper.reset(); write(mapper, 0x5209, 4); mapper.tick();
    write(mapper, 0x520A, 1); mapper.tick(); write(mapper, 0x5209, 1);
    const auto running = state(mapper);
    for (unsigned i = 0; i < 256; ++i) mapper.tick();
    check(!mapper.getIrqFlag(), "Active timer byte overwrite lost high byte");
    mapper.tick(); check(mapper.getIrqFlag(), "Active timer byte overwrite did not take effect");
    std::stringstream restore(running); mapper.loadState(restore);
    check(restore.good() && state(mapper) == running, "Active MMC5A timer state lost");
    for (unsigned i = 0; i < 257; ++i) mapper.tick();
    check(mapper.getIrqFlag(), "Restored timer expiry changed");
    const auto expired = state(mapper);
    cpuByte(mapper, 0x5209); std::stringstream restoreExpired(expired); mapper.loadState(restoreExpired);
    check(restoreExpired.good() && mapper.getIrqFlag(), "Pending timer IRQ state lost");

    pendingIrq(mapper); write(mapper, 0x5209, 1); mapper.tick();
    cpuByte(mapper, 0x5209);
    check(mapper.getIrqFlag() && (irqStatus(mapper) & 0x80), "Timer acknowledge cleared scanline IRQ");
    write(mapper, 0x5209, 1); mapper.tick(); cpuByte(mapper, 0x5204);
    check(mapper.getIrqFlag(), "Scanline acknowledge cleared concurrent timer IRQ");
    cpuByte(mapper, 0x5209); check(!mapper.getIrqFlag(), "Both IRQ sources did not release line");

    mapper.reset(); mapper.setExpansionInputs(0x80);
    check(cpuByte(mapper, 0x5208) == 0x80, "GPIO input levels ignored");
    write(mapper, 0x5208, 0x40); write(mapper, 0x5207, 0);
    check(cpuByte(mapper, 0x5208) == 0x40, "GPIO output latch ignored");
    write(mapper, 0x5207, 0x80);
    check(cpuByte(mapper, 0x5208) == 0xC0, "GPIO mixed direction incorrect");
    write(mapper, 0x5207, 0xFF);
    check(mapper.expansionPins() == 0xC0, "Peripheral function did not override GPIO");
    for (uint16_t addr : {0x5800, 0x5900, 0x5BFF})
    {
        check(mapper.expansionPins(addr, false, true) == 0x80 &&
              mapper.expansionPins(addr, true, true) == 0x40 &&
              mapper.expansionPins(addr, false, false) == 0xC0, "Peripheral CL3/SL3 phase decode wrong");
        data = 0x96;
        check(!mapper.cpuMapRead(addr, mapped, data) && data == 0x96, "Invented peripheral RAM drove bus");
        check(mapper.cpuMapWrite(addr, mapped, 0x11, 0) && mapped == 0xFFFFFFFF,
              "Peripheral write not consumed");
    }
    check(mapper.expansionPins(0x57FF, false, true) == 0xC0 &&
          mapper.expansionPins(0x5C00, true, true) == 0xC0, "Peripheral range leaked");
    const auto pins = state(mapper);
    mapper.setExpansionInputs(0); write(mapper, 0x5207, 0);
    std::stringstream restorePins(pins); mapper.loadState(restorePins);
    check(restorePins.good() && state(mapper) == pins, "GPIO state lost");
    for (const auto field : {std::pair<size_t, uint8_t>{67, 2}, {68, 4}, {69, 1}, {70, 1}, {73, 2}, {74, 2}})
    {
        auto invalid = pins; invalid[field.first] = field.second;
        std::stringstream input(invalid); mapper.loadState(input);
        check(input.fail() && state(mapper) == pins, "Invalid MMC5A state changed mapper");
    }
    std::stringstream incompatible(pins); const auto before = state(original); original.loadState(incompatible);
    check(incompatible.fail() && state(original) == before, "Cross-revision state accepted");
    std::stringstream truncated(pins.substr(0, 74)); mapper.loadState(truncated);
    check(truncated.fail() && state(mapper) == pins, "Truncated MMC5A state changed mapper");
    config.revision = static_cast<Mapper005::Revision>(2);
    check(!Mapper005::supports(32, 128, config), "Unknown silicon revision accepted");
}
static void hardwareResets()
{
    for (auto revision : {Mapper005::Revision::MMC5, Mapper005::Revision::MMC5A})
    {
        Mapper005::Configuration config; config.revision = revision;
        Mapper005 mapper(32, 128, config);
        pendingIrq(mapper);
        write(mapper, 0x5102, 2); write(mapper, 0x5103, 1); write(mapper, 0x6000, 0x96);
        write(mapper, 0x5104, 2); write(mapper, 0x5C00, 0xA5);
        write(mapper, 0x5101, 3); write(mapper, 0x5130, 1); write(mapper, 0x5120, 7);
        write(mapper, 0x5100, 0); write(mapper, 0x5117, 0x81);
        write(mapper, 0x5200, 0xC8); write(mapper, 0x5201, 17);
        write(mapper, 0x5202, 9); write(mapper, 0x5205, 17); write(mapper, 0x5206, 19);
        mapper.setExpansionInputs(0x80);
        write(mapper, 0x5207, 0); write(mapper, 0x5208, 0x40);
        write(mapper, 0x5209, 1); mapper.tick();
        // Compara todo o estado: somente os campos com reset documentado devem mudar.
        auto expected = state(mapper);
        expected[0] = 3; expected[2] = 1; expected[3] = 2; expected[4] = 3;
        expected[12] = char(0xFF); expected[37] = 0; expected[41] = 0;
        expected[68] = char(0xC0); expected[69] = 0;
        for (size_t field = 71; field <= 74; ++field) expected[field] = 0;
        expected[expected.size() - 10] = 0; expected[expected.size() - 9] = 0;
        expected[expected.size() - 3] = 0;
        mapper.hardwareReset();
        check(state(mapper) == expected, "Hardware reset changed undocumented state or missed a reset field");
        check(!mapper.getIrqFlag() && (irqStatus(mapper) & 0x80), "Hardware reset erased pending scanline IRQ");
        write(mapper, 0x5204, 0x80); check(mapper.getIrqFlag(), "Pending scanline IRQ not re-enabled after reset");
        check(cpuByte(mapper, 0x5C00) == 0xA5 && cpuByte(mapper, 0x6000) == 0x96, "Hardware reset erased memory");
        write(mapper, 0x6000, 0); check(cpuByte(mapper, 0x6000) == 0x96, "Hardware reset failed to protect WRAM");
        check(cpuByte(mapper, 0x5205) == (17 * 19 & 255), "Hardware reset erased multiplier");
        mapper.reset();
        check(irqStatus(mapper) == 0 && !mapper.getIrqFlag(), "Complete reset preserved scanline state");
        if (revision == Mapper005::Revision::MMC5A)
        {
            write(mapper, 0x5209, 20); mapper.hardwareReset();
            for (unsigned i = 0; i < 30; ++i) mapper.tick();
            check(!mapper.getIrqFlag() && cpuByte(mapper, 0x5209) == 0, "Hardware reset failed to stop active timer");
        }
    }
}
static std::string consoleState(NES &nes)
{
    std::stringstream output;
    check(nes.saveState(output), "Console save failed");
    return output.str();
}
static void ppuIntegration(NES &nes)
{
    auto &bus = nes.getBus(); auto &ppu = nes.getPpu();
    auto &mapper = *bus.cart->getMapper();
    for (unsigned i = 0; i < 90000; ++i) ppu.clock(); // Fim do bloqueio de escrita inicial.
    bus.cpuWrite(0x2001, 0); bus.cpuWrite(0x2000, 0x20);
    bus.cpuWrite(0x5101, 3); bus.cpuWrite(0x5130, 0);
    for (unsigned reg = 0; reg < 8; ++reg) bus.cpuWrite(0x5120 + reg, 1);
    for (unsigned reg = 0; reg < 4; ++reg) bus.cpuWrite(0x5128 + reg, 2);
    const auto readData = [&](uint16_t address)
    {
        bus.cpuRead(0x2002);
        bus.cpuWrite(0x2006, address >> 8); bus.cpuWrite(0x2006, address & 255);
        bus.cpuRead(0x2007); // Primeira leitura devolve o buffer anterior.
        return bus.cpuRead(0x2007);
    };
    check(readData(0) == 0 && readData(8) == 0xFF, "PPUDATA did not use last-written B with rendering disabled");
    const auto lastB = consoleState(nes);
    bus.cpuWrite(0x5120, 1);
    check(readData(0) == 0xFF && readData(8) == 0, "PPUDATA did not switch to last-written A");
    std::stringstream restore(lastB);
    check(nes.loadState(restore) && readData(8) == 0xFF, "Last-written B/PPU state not restored");
    bus.cpuWrite(0x2008, 0); // A PPU muda para 8x8, mas o MMC5 ainda observa 8x16.
    check(readData(0) == 0, "PPUCTRL mirror changed MMC5 mode");
    bus.cpuWrite(0x2000, 0);
    check(readData(0) == 0xFF, "Exact PPUCTRL write did not select A in 8x8 mode");

    bus.cpuWrite(0x2000, 0x20); bus.cpuWrite(0x2001, 0x0A);
    bus.cpuWrite(0x5120, 1); // Janela de PPUDATA fica em A; background deve continuar em B.
    bus.cpuWrite(0x2009, 0); // Desliga a PPU pelo espelho, sem desligar as substituições do MMC5.
    uint8_t data = 0;
    check(bus.ppuRead(0, data, PpuReadContext::Background) && data == 0, "PPUMASK mirror changed MMC5 substitutions");
    bus.cpuWrite(0x2001, 0);
    check(bus.ppuRead(0, data, PpuReadContext::Background) && data == 0xFF, "Exact PPUMASK write did not disable substitutions");
    for (unsigned i = 0; i < 4; ++i) ppu.clock();
    bus.cpuWrite(0x5105, 0); bus.cpuWrite(0x5104, 0);
    for (unsigned address = 0x2000; address < 0x2400; ++address) ppu.ppuWrite(address, 0);
    ppu.ppuWrite(0x3F01, 0x01); ppu.ppuWrite(0x3F02, 0x21); ppu.ppuWrite(0x3F07, 0x31);
    bus.cpuRead(0x2002); bus.cpuWrite(0x2005, 0); bus.cpuWrite(0x2005, 0);
    bus.cpuWrite(0x2001, 0x0A);
    const auto pixelAfterFrames = [&]()
    {
        for (unsigned i = 0; i < 2 * 341 * 262; ++i) ppu.clock();
        return ppu.getFrameBuffer()[120 * 256 + 128];
    };
    check(pixelAfterFrames() == PPU::getSystemPalette()[0x21], "Real background fetch used PPUDATA/A bank instead of B");
    const auto beforeViewer = state(mapper);
    ppu.getPatternTablePixels(0, 0);
    check(state(mapper) == beforeViewer, "Pattern viewer mutated MMC5 state");
    bus.cpuWrite(0x5104, 2);
    for (unsigned address = 0x5C00; address < 0x5FC0; ++address) bus.cpuWrite(address, 0x41);
    bus.cpuWrite(0x5104, 1);
    check(pixelAfterFrames() == PPU::getSystemPalette()[0x31], "PPU did not render extended CHR/palette from CIRAM tile");
    bus.cpuWrite(0x5105, 0xFF); bus.cpuWrite(0x5106, 0); bus.cpuWrite(0x5107, 2);
    check(pixelAfterFrames() == PPU::getSystemPalette()[0x31], "PPU fill mode ignored extended palette");
}
static void splitIntegration(NES &nes)
{
    auto &bus = nes.getBus(); auto &ppu = nes.getPpu();
    auto &mapper = *static_cast<Mapper005 *>(bus.cart->getMapper().get());
    ppu.reset(); mapper.reset();
    for (unsigned i = 0; i < 341 * 262; ++i) ppu.clock();
    bus.cpuWrite(0x2000, 0x20); bus.cpuWrite(0x5101, 3);
    for (unsigned reg = 0; reg < 8; ++reg) bus.cpuWrite(0x5120 + reg, 1);
    for (unsigned reg = 0; reg < 4; ++reg) bus.cpuWrite(0x5128 + reg, 2);
    bus.cpuWrite(0x5104, 2); bus.cpuWrite(0x5105, 0);
    for (unsigned address = 0x2000; address < 0x2400; ++address) ppu.ppuWrite(address, 0);
    for (unsigned row = 0; row < 30; ++row)
        for (unsigned column = 0; column < 32; ++column)
            bus.cpuWrite(0x5C00 + row * 32 + column, row & 1 ? 255 : 0);
    for (unsigned offset = 0x3C0; offset < 1024; ++offset) bus.cpuWrite(0x5C00 + offset, 0xE4);
    ppu.ppuWrite(0x3F00, 0x0F);
    for (unsigned entry = 1; entry < 16; ++entry) ppu.ppuWrite(0x3F00 + entry, 0x10 + entry);
    bus.cpuWrite(0x5104, 0); bus.cpuWrite(0x5202, 1);
    for (bool right : {false, true})
        for (unsigned threshold : {0, 1, 8, 31})
            for (unsigned fineX : {0, 3, 7})
                for (unsigned scroll : {0, 8})
                {
                    bus.cpuWrite(0x2001, 0);
                    for (unsigned i = 0; i < 4; ++i) ppu.clock();
                    bus.cpuRead(0x2002);
                    bus.cpuWrite(0x2005, (5 * 8) + fineX); // Scroll normal não deve mover o split.
                    bus.cpuWrite(0x2005, 0);
                    bus.cpuWrite(0x5200, 0x80 | (right ? 0x40 : 0) | threshold);
                    bus.cpuWrite(0x5201, scroll); bus.cpuWrite(0x2001, 0x0A);
                    for (unsigned i = 0; i < 2 * 341 * 262; ++i) ppu.clock();
                    for (unsigned y : {64, 72, 120, 128})
                        for (unsigned x : {0, 3, 7, 8, 15, 63, 64, 127, 128, 239, 248, 255})
                        {
                            const unsigned column = (x + fineX) / 8;
                            const bool split = right ? column >= threshold : column < threshold;
                            const unsigned tileRow = ((y + scroll) % 240) / 8;
                            const unsigned palette = ((tileRow & 2) ? 2 : 0) | ((column & 2) ? 1 : 0);
                            const unsigned color = !split ? 0x12 : (tileRow & 1) ? 0x0F : 0x10 + palette * 4 + 3;
                            check(ppu.getFrameBuffer()[y * 256 + x] == PPU::getSystemPalette()[color],
                                  "Real PPU split boundary/scroll/palette pixel mismatch");
                        }
                }
    // Snapshot durante rendering deve preservar região, linha e tile preparado.
    for (unsigned i = 0; i < 123; ++i) ppu.clock();
    const auto checkpoint = consoleState(nes);
    for (unsigned i = 0; i < 1500; ++i) ppu.clock();
    const auto after = consoleState(nes);
    std::stringstream restore(checkpoint);
    check(nes.loadState(restore), "Active split console state rejected");
    for (unsigned i = 0; i < 1500; ++i) ppu.clock();
    check(consoleState(nes) == after, "Active split restore changed future console state/pixels");
    std::stringstream cartState; bus.cart->saveState(cartState);
    const size_t start = after.size() - state(mapper).size() - cartState.str().size();
    for (size_t field : {size_t(0), size_t(2), size_t(4), size_t(7)})
    {
        auto damaged = after; damaged[start + field] = 4;
        std::stringstream invalid(damaged);
        check(!nes.loadState(invalid) && consoleState(nes) == after, "Semantic mapper failure did not roll back console");
    }
}
static void irqIntegration(NES &nes)
{
    auto &bus = nes.getBus(); auto &ppu = nes.getPpu();
    auto &mapper = *static_cast<Mapper005 *>(bus.cart->getMapper().get());
    for (uint8_t ctrl : {0, 0x10, 0x20, 0x30})
        for (uint8_t mask : {8, 0x10, 0x18})
            for (bool unlimited : {false, true})
            {
                ppu.reset(); mapper.reset(); ppu.setUnlimitedSprites(unlimited);
                // Um quadro sem rendering termina no pré-render, dot 0, quadro ímpar.
                for (unsigned i = 0; i < 341 * 262; ++i) ppu.clock();
                bus.cpuWrite(0x2000, ctrl); bus.cpuWrite(0x2001, mask);
                bus.cpuWrite(0x5203, 1); bus.cpuWrite(0x5204, 0x80);
                unsigned dots = 0;
                const auto dot = [&]() { ppu.clock(); if (++dots % 3 == 0) mapper.tick(); };
                for (unsigned i = 0; i < 340; ++i) dot(); // Pré-render ímpar pula dot 340.
                for (unsigned cycle = 0; cycle < 4; ++cycle)
                {
                    dot(); check(irqStatus(mapper) == 0, "Real PPU started frame before dot 4");
                }
                dot(); check(irqStatus(mapper) == 0x40, "Real PPU did not detect scanline zero at dot 4");
                for (unsigned cycle = 5; cycle <= 340; ++cycle) dot();
                for (unsigned cycle = 0; cycle < 4; ++cycle)
                {
                    dot(); check(!mapper.getIrqFlag(), "Real PPU IRQ fired before dot 4");
                }
                // Salva entre o fetch lógico do atributo e o pulso /RD do dot 4.
                const auto checkpoint = consoleState(nes); const unsigned savedDots = dots;
                dot(); check(mapper.getIrqFlag() && irqStatus(mapper) == 0xC0, "Real PPU IRQ missed scanline 1 dot 4");
                const auto after = consoleState(nes);
                std::stringstream restore(checkpoint);
                check(nes.loadState(restore), "Mid-fetch console state rejected");
                dots = savedDots; dot();
                check(consoleState(nes) == after, "Restoring pending /RD shifted scanline IRQ");
                check(bus.cpuRead(0x5204, true) == 0xC0 && mapper.getIrqFlag(), "Bus debugger acknowledged IRQ");
                const auto beforeVectors = state(mapper);
                bus.cpuRead(0xFFFA, true); bus.cpuRead(0xFFFB, true);
                check(state(mapper) == beforeVectors, "Bus debugger NMI vector reset frame");
                check(bus.cpuRead(0x5204) == 0xC0 && !mapper.getIrqFlag(), "Real CPU read did not acknowledge IRQ");
                bus.cpuWrite(0x5203, 2);
                for (unsigned cycle = 5; cycle <= 340; ++cycle) dot();
                for (unsigned cycle = 0; cycle < 4; ++cycle) dot();
                check(!mapper.getIrqFlag(), "Additional sprite reads advanced IRQ before dot 4");
                dot(); check(mapper.getIrqFlag(), "Real PPU scanline 2/Unlimited Sprites detector mismatch");
            }
    // O valor de PPUSTATUS precisa atravessar PPU -> Bus -> mapper sem ser consumido.
    const auto reachVblank = [&]()
    {
        unsigned clocks = 0;
        while (!(ppu.cpuRead(0x2002, true) & 0x80) && clocks++ < 90000) ppu.clock();
        check(ppu.cpuRead(0x2002, true) & 0x80, "Vblank setup timed out");
    };
    reachVblank(); pendingIrq(mapper);
    check((bus.cpuRead(0x2002, true) & 0x80) && mapper.getIrqFlag(), "PPUSTATUS debugger reset MMC5");
    check((bus.cpuRead(0x200A) & 0x80) && mapper.getIrqFlag(), "PPUSTATUS mirror reset MMC5");
    reachVblank(); pendingIrq(mapper);
    check((bus.cpuRead(0x2002) & 0x80) && !mapper.getIrqFlag() && irqStatus(mapper) == 0,
          "Observed PPUSTATUS value did not reset MMC5");
    pendingIrq(mapper); bus.cpuWrite(0x4014, 0);
    check(bus.dma_transfer && !mapper.getIrqFlag(), "MMC5 observation consumed OAM DMA or missed reset");
    for (unsigned i = 0; i < 4; ++i) nes.step();
    check(irqStatus(mapper) == 0, "Mapper did not remain idle during vblank DMA");
    pendingIrq(mapper); bus.cpuRead(0xFFFA);
    check(irqStatus(mapper) == 0, "Actual NMI vector bus read did not reset MMC5");
}
static void exRamAndDebugIntegration(NES &nes)
{
    auto &bus = nes.getBus(); auto &ppu = nes.getPpu();
    auto &mapper = *static_cast<Mapper005 *>(bus.cart->getMapper().get());
    bus.dma_transfer = false;
    for (unsigned mode = 0; mode < 4; ++mode)
        for (bool rendering : {false, true})
        {
            mapper.reset(); bus.cpuWrite(0x5104, 2); bus.cpuWrite(0x5FFF, 0x5A);
            if (rendering) detectedLine(mapper);
            bus.cpuWrite(0x5104, mode); bus.cpuWrite(0, 0x96);
            check(bus.cpuRead(0x5FFF, true) == (mode < 2 ? 0x96 : 0x5A) && bus.cpuDataBus == 0x96,
                  "ExRAM debugger changed bus or returned wrong data");
            check(bus.cpuRead(0x5FFF) == (mode < 2 ? 0x96 : 0x5A), "ExRAM real bus open/read mismatch");
            bus.cpuWrite(0x5FFF, 0xA5); bus.cpuWrite(0x5104, 2);
            check(bus.cpuRead(0x5FFF) == ((mode == 2 || (mode < 2 && rendering)) ? 0xA5 : 0x5A),
                  "ExRAM real bus write mismatch");
        }
    // PPUDATA real usa ExRAM em modos 0/1 mesmo durante blanking.
    bus.cpuWrite(0x2001, 0); for (unsigned i = 0; i < 4; ++i) ppu.clock();
    bus.cpuWrite(0x5105, 0xAA);
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        bus.cpuWrite(0x5104, 2); bus.cpuWrite(0x5C11, 0x5A); bus.cpuWrite(0x5104, mode);
        const auto address = [&]()
        {
            bus.cpuRead(0x2002); bus.cpuWrite(0x2006, 0x20); bus.cpuWrite(0x2006, 0x11);
        };
        address(); bus.cpuWrite(0x2007, 0xC3);
        address(); bus.cpuRead(0x2007);
        check(bus.cpuRead(0x2007) == (mode < 2 ? 0xC3 : 0), "Real PPUDATA ExRAM permission mismatch");
        bus.cpuWrite(0x5104, 2);
        check(bus.cpuRead(0x5C11) == (mode < 2 ? 0xC3 : 0x5A), "PPUDATA write bypassed ExRAM mode");
    }
    // Consulta de estado completo, com IRQ ativa, tile ExGrafix e sequência parcial.
    mapper.reset(); write(mapper, 0x5104, 2); write(mapper, 0x5C22, 0xC1);
    write(mapper, 0x5104, 1); write(mapper, 0x2000, 0x20); write(mapper, 0x2001, 0x18);
    write(mapper, 0x5203, 1); write(mapper, 0x5204, 0x80);
    detectedLine(mapper); detectedLine(mapper);
    uint8_t data = 0; ppuRead(mapper, 0x2022, PpuReadContext::Background, data);
    mapper.onPpuRead(0x2022); mapper.onPpuRead(0x2022);
    const auto before = consoleState(nes);
    for (uint16_t address : {0, 0x2002, 0x2007, 0x200A, 0x5000, 0x5204, 0x5205, 0x5206, 0x5C22, 0x8000, 0xFFFA, 0xFFFB})
    {
        bus.cpuRead(address, true); nes.cpuRead(address);
    }
    for (uint16_t address : {0, 0x1FFF, 0x2022, 0x2043, 0x23C0, 0x2FFF, 0x3000, 0x3EFF}) bus.ppuPeek(address, data);
    ppu.getPatternTablePixels(0, 0); ppu.getPatternTablePixels(1, 3);
    check(consoleState(nes) == before && mapper.getIrqFlag(), "Debugger/viewer changed console state or IRQ");
    // A sequência parcial precisa continuar intacta após todas as consultas.
    write(mapper, 0x5203, 2); bus.cpuRead(0x5204);
    mapper.onPpuRead(0x2022);
    check(!mapper.getIrqFlag(), "Debugger advanced partial scanline detector");
    mapper.onPpuRead(0x23C0);
    check(mapper.getIrqFlag(), "Debugger discarded partial scanline detector");
}
static void memoryHeaders(const std::filesystem::path &dir)
{
    const auto image = [](unsigned prg, unsigned chr, bool nes2, uint8_t ram = 0, bool trainer = false)
    {
        std::vector<uint8_t> bytes(16 + (trainer ? 512 : 0) + prg * 16384 + chr * 8192, 0);
        bytes[0] = 'N'; bytes[1] = 'E'; bytes[2] = 'S'; bytes[3] = 0x1A;
        bytes[4] = prg; bytes[5] = chr; bytes[6] = 0x50 | (trainer ? 4 : 0);
        bytes[7] = nes2 ? 8 : 0; bytes[8] = nes2 ? 0 : ram;
        if (nes2) { bytes[10] = ram; bytes[11] = chr ? 0 : 7; }
        if (trainer) for (unsigned i = 0; i < 512; ++i) bytes[16 + i] = i ^ 0xA5;
        return bytes;
    };
    const auto path = dir / "mapper005-memory.nes";
    const auto save = [&](const std::vector<uint8_t> &bytes)
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        check(output.good(), "Memory fixture write failed");
        return path.string();
    };
    const auto reject = [&](const std::vector<uint8_t> &bytes)
    {
        Cartridge cart(save(bytes));
        check(!cart.isValid() && !cart.getMapper(), "Unsupported/truncated MMC5 image accepted");
    };
    for (bool nes2 : {false, true})
    {
        for (unsigned prg : {1, 2, 8, 64})
            for (unsigned chr : {0, 1, 16, 128})
            {
                Cartridge cart(save(image(prg, chr, nes2)));
                check(cart.isValid(), "Valid MMC5 ROM capacity rejected");
            }
        for (unsigned prg : {0, 3, 65}) reject(image(prg, 1, nes2));
        for (unsigned chr : {3, 129}) reject(image(8, chr, nes2));
        auto bytes = image(8, 16, nes2); bytes.pop_back(); reject(bytes);
        bytes = image(8, 16, nes2); bytes.resize(16 + 8 * 16384 - 1); reject(bytes);
        bytes = image(8, 16, nes2); bytes[6] |= 8; reject(bytes);
    }
    for (uint8_t ram : {0, 7, 8, 9, 10, 11, 0x70, 0x77, 0x79, 0x97, 0x99})
    {
        Cartridge cart(save(image(8, 16, true, ram)));
        check(cart.isValid(), "Valid MMC5 RAM declaration rejected");
        const auto decoded = [](unsigned shift) { return shift ? 64u << shift : 0; };
        const unsigned total = decoded(ram & 15) + decoded(ram >> 4);
        check(state(*cart.getMapper()).size() == 3180 + total, "Cartridge RAM declaration ignored");
        uint8_t data = 0x96;
        check(cart.cpuRead(0x6000, data) && data == (total ? 0 : 0x96), "Declared zero WRAM not open bus");
    }
    for (uint8_t ram : {1, 6, 12, 0x66, 0x78, 0x88, 0xA7, 0xBB}) reject(image(8, 16, true, ram));
    for (unsigned sub = 1; sub < 16; ++sub)
    {
        auto bytes = image(8, 16, true); bytes[8] = sub << 4; reject(bytes);
    }
    for (unsigned field : {11, 14})
    {
        auto bytes = image(8, 16, true); bytes[field] = 1; reject(bytes);
    }
    auto exponent = image(1, 1, true); exponent[4] = 14 << 2; exponent[5] = 13 << 2; exponent[9] = 0xFF;
    Cartridge encoded(save(exponent)); check(encoded.isValid(), "NES 2.0 exponent size rejected");
    for (bool nes2 : {false, true})
    {
        auto bytes = image(8, 16, nes2, nes2 ? 7 : 1, true);
        Cartridge cart(save(bytes)); check(cart.isValid(), "Valid MMC5 trainer image rejected");
        for (unsigned i = 0; i < 512; ++i)
        {
            uint8_t data = 0;
            check(cart.cpuRead(0x7000 + i, data) && data == uint8_t(i ^ 0xA5), "Cartridge trainer not installed");
        }
        cart.cpuWrite(0x7000, 0, 0); uint8_t data = 0;
        check(cart.cpuRead(0x7000, data) && data == 0xA5, "Trainer installation bypassed persistent write protection");
        bytes.resize(16 + 511); reject(bytes);
    }
    reject(image(8, 16, true, 0, true));
    for (uint8_t legacyRam : {0, 1, 2, 4, 8, 16})
    {
        Cartridge cart(save(image(8, 16, false, legacyRam)));
        check(cart.isValid(), "Valid explicit/legacy iNES WRAM rejected");
        check(state(*cart.getMapper()).size() == 3180 + (legacyRam ? legacyRam * 8192 : 65536), "iNES WRAM fallback/explicit size mismatch");
    }
    for (const auto dimensions : {std::pair<unsigned, unsigned>{0, 1}, {3, 1}, {65, 1}, {1, 3}, {1, 129}})
    {
        bool rejected = false;
        try { Mapper005 mapper(dimensions.first, dimensions.second); }
        catch (const std::invalid_argument &) { rejected = true; }
        check(rejected, "Direct unsupported MMC5 dimensions accepted");
    }
}
static void ciramIntegration(NES &nes)
{
    auto &ppu = nes.getPpu(); auto &bus = nes.getBus();
    bus.cpuWrite(0x2001, 0); bus.cpuWrite(0x5104, 0); bus.cpuWrite(0x5105, 0x44);
    ppu.getVram().writeRaw(0x12, 0xA5); ppu.getVram().writeRaw(0x412, 0x5A);
    check(ppu.ppuRead(0x2012) == 0xA5 && ppu.ppuRead(0x2812) == 0xA5 &&
          ppu.ppuRead(0x2412) == 0x5A && ppu.ppuRead(0x2C12) == 0x5A,
          "Raw VRAM viewer and MMC5 CIRAM are different memories");
    ppu.ppuWrite(0x2412, 0xC3);
    check(ppu.getVram().readRaw(0x412) == 0xC3, "Mapper CIRAM write not visible to raw viewer");
    const auto snapshot = consoleState(nes);
    ppu.getVram().writeRaw(0x412, 0);
    std::stringstream restore(snapshot);
    check(nes.loadState(restore) && consoleState(nes) == snapshot && ppu.getVram().readRaw(0x412) == 0xC3,
          "CIRAM snapshot did not restore authoritative viewer memory");
}
static void mmc5aIntegration(NES &nes)
{
    nes.reset();
    auto &bus = nes.getBus();
    auto &mapper = *static_cast<Mapper005 *>(bus.cart->getMapper().get());
    // O header não distingue silício: o teste instala explicitamente a revisão A.
    Mapper005::Configuration config; config.revision = Mapper005::Revision::MMC5A;
    mapper = Mapper005(2, 128, config);
    bus.cpuWrite(0x5209, 10); bus.cpuWrite(0x4014, 0);
    for (unsigned i = 0; i < 8; ++i) nes.step();
    check(bus.dma_transfer && !mapper.getIrqFlag(), "Timer during DMA expired early");
    const auto snapshot = consoleState(nes);
    nes.step(); check(!mapper.getIrqFlag(), "Timer did not use system M2 cadence");
    nes.step(); check(mapper.getIrqFlag(), "DMA stalled MMC5A timer");
    const auto expired = consoleState(nes);
    check(bus.cpuRead(0x5209, true) == 0x80 && consoleState(nes) == expired,
          "Console timer peek acknowledged IRQ or changed state");
    bus.cpuRead(0x5204); check(mapper.getIrqFlag(), "Console scanline read acknowledged timer");
    check(bus.cpuRead(0x5209) == 0x80 && !mapper.getIrqFlag(), "Console timer read did not acknowledge IRQ");
    std::stringstream restore(snapshot); check(nes.loadState(restore), "Console MMC5A state rejected");
    nes.step(); nes.step();
    check(consoleState(nes) == expired, "MMC5A state changed future console execution");
    std::stringstream cartState; bus.cart->saveState(cartState);
    const size_t mapperStart = expired.size() - state(mapper).size() - cartState.str().size();
    auto invalid = expired; invalid[mapperStart + 67] = 0;
    std::stringstream wrongRevision(invalid);
    check(!nes.loadState(wrongRevision) && consoleState(nes) == expired,
          "Console accepted cross-revision state or failed rollback");
    mapper = Mapper005(2, 128);
}
static void console(const std::filesystem::path &dir)
{
    for (unsigned chrSize : {0u, 128u})
    {
        std::vector<uint8_t> rom(16 + 32768 + chrSize * 8192, 0);
        rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
        rom[4] = 2; rom[5] = chrSize; rom[6] = 0x50;
        for (unsigned page = 0; page < chrSize * 8; ++page)
        {
            rom[16 + 32768 + page * 1024] = page & 255;
            rom[16 + 32768 + page * 1024 + 1] = page >> 8;
        }
        if (chrSize)
            for (unsigned page : {1, 2, 4})
                for (unsigned offset = 0; offset < 1024; ++offset)
                    rom[16 + 32768 + page * 1024 + offset] =
                        page == 4 || ((offset & 15) < 8 ? page == 1 : page == 2) ? 0xFF : 0;
        const auto path = dir / "mapper005-fixture.nes";
        { std::ofstream output(path, std::ios::binary);
          output.write(reinterpret_cast<const char *>(rom.data()), rom.size());
          check(output.good(), "Fixture write failed"); }
        NES nes; nes.insertCartridge(path.string());
        check(nes.isCartridgeLoaded(), "MMC5 fixture rejected");
        auto &cart = *nes.getBus().cart;
        auto &mapper = *static_cast<Mapper005 *>(cart.getMapper().get());
        write(mapper, 0x5101, 3); write(mapper, 0x5130, 1); write(mapper, 0x5120, 7);
        write(mapper, 0x5130, 2);
        uint8_t data = 0;
        if (chrSize)
        {
            check(cart.ppuRead(0, data, 0) && data == 7, "Cartridge CHR low byte mismatch");
            check(cart.ppuRead(1, data, 0) && data == 1, "Cartridge CHR high byte mismatch");
        }
        else check(cart.ppuWrite(0, 0xA5, 0), "CHR RAM write rejected");
        const auto saved = consoleState(nes);
        check(uint8_t(saved[0]) == 0x49, "Missing R2NI state version");
        std::stringstream cartState; cart.saveState(cartState);
        const size_t start = saved.size() - state(mapper).size() - cartState.str().size();
        for (uint8_t oldVersion : {0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48})
        {
            auto old = saved; old[0] = oldVersion;
            if (oldVersion == 0x42) old.erase(8, 5);
            std::stringstream legacy(old);
            check(!nes.loadState(legacy) && consoleState(nes) == saved, "Old console state accepted or changed console");
        }
        for (size_t length : {start, start + 14, start + 37, saved.size() - 1})
        {
            std::stringstream truncated(saved.substr(0, length));
            check(!nes.loadState(truncated) && consoleState(nes) == saved, "Console rollback failed");
        }
        auto damaged = saved; damaged[start + 14] = 4;
        std::stringstream broken(damaged);
        check(!nes.loadState(broken) && consoleState(nes) == saved, "Invalid CHR state did not roll back console");
        nes.reset(); cart.ppuWrite(0, 0x11, 0);
        std::stringstream restore(saved);
        check(nes.loadState(restore) && consoleState(nes) == saved, "Console MMC5/CHR state restore failed");
        if (!chrSize) check(cart.ppuRead(0, data, 0) && data == 0xA5, "CHR RAM suffix misaligned");
        else { ppuIntegration(nes); splitIntegration(nes); irqIntegration(nes); exRamAndDebugIntegration(nes); ciramIntegration(nes); mmc5aIntegration(nes); }
    }
}
int main(int argc, char **argv)
{
    try
    {
        check(argc == 2, "Pass a directory for generated ROMs");
        std::filesystem::create_directories(argv[1]);
        banking(); states(); contexts(); extendedAttributes(); scanlineIrqs(); initialMemoryModes(); exRamPermissions(); strictStatesAndSplit(); wramAndRegisters(); mmc5a(); hardwareResets(); memoryHeaders(argv[1]); console(argv[1]);
        std::cout << "MMC5 batches 1-7 passed (CHR, IRQ, ExRAM, debugger, states, split, WRAM, headers/trainers, MMC5A/reset)\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
