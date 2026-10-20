#include "Core/PPU/PPU.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Modelo de referência simples: oito decrementos por dot, como antes da otimização.
struct Fixture
{
    R2NES::Core::PPU ppu;
    std::array<uint32_t, 8> remaining{};
    uint8_t latch = 0;

    void drive(uint8_t value, uint8_t mask)
    {
        latch = (latch & ~mask) | (value & mask);
        for (unsigned bit = 0; bit < 8; ++bit)
            if (mask & (1u << bit))
                remaining[bit] = 64000;
    }

    void verify()
    {
        // Leitura de registrador somente de escrita não recarrega o latch.
        check(ppu.cpuRead(0x2000, true) == latch, "Open bus differs from per-dot reference");
    }

    void clock(unsigned count)
    {
        for (unsigned dot = 0; dot < count; ++dot)
        {
            ppu.clock();
            for (unsigned bit = 0; bit < 8; ++bit)
                if (remaining[bit] > 0 && --remaining[bit] == 0)
                    latch &= ~(1u << bit);
            verify();
        }
    }

    void write(uint16_t address, uint8_t value)
    {
        ppu.cpuWrite(address, value);
        drive(value, 0xFF);
        verify();
    }

    void status()
    {
        const auto value = ppu.cpuRead(0x2002);
        check((value & 0x1F) == (latch & 0x1F), "Status lost retained low bits");
        drive(value, 0xE0);
        verify();
    }

    void palette()
    {
        const auto value = ppu.cpuRead(0x2007);
        check(value == (0x3F | (latch & 0xC0)), "Palette read lost retained high bits");
        drive(value, 0x3F);
        verify();
    }

    void roundTrip()
    {
        std::ostringstream output(std::ios::binary | std::ios::out);
        ppu.saveState(output);
        check(output.good(), "PPU snapshot failed");
        // Descarta uma linha temporal intermediária antes de restaurar o snapshot.
        for (int dot = 0; dot < 29; ++dot)
            ppu.clock();
        ppu.cpuWrite(0x2002, 0xAA);
        std::istringstream input(output.str(), std::ios::binary | std::ios::in);
        ppu.loadState(input);
        check(input.good(), "PPU snapshot restore failed");
        verify();
        std::ostringstream restored;
        ppu.saveState(restored);
        check(restored.str() == output.str(), "PPU snapshot bytes changed after restore");
    }
};

int main()
{
    try
    {
        Fixture f;
        f.write(0x2002, 0xFF);
        f.clock(12345);
        f.status();
        f.roundTrip();
        f.clock(51654); // Um dot antes do vencimento dos bits baixos.
        f.roundTrip();
        f.clock(1);
        f.roundTrip();
        f.clock(19999);
        f.status(); // Recarrega apenas os bits altos durante VBlank.
        f.clock(1);
        f.roundTrip();
        f.clock(64000);
        f.ppu.reset();
        f.remaining.fill(0);
        f.latch = 0;
        f.clock(90000); // Libera o lockout dos registradores após reset.
        f.ppu.ppuWrite(0x3F00, 0x3F);
        f.ppu.ppuWrite(0x3F01, 0x3F);
        f.write(0x2006, 0x3F);
        f.write(0x2006, 0x00);
        f.write(0x2002, 0xFF);
        f.palette();
        f.clock(20000);
        f.palette(); // Recarrega só os seis bits baixos.
        f.roundTrip();
        f.clock(43999);
        f.roundTrip();
        f.clock(1);
        check(f.latch == 0x3F, "Upper bits did not expire independently");
        f.clock(19999);
        f.roundTrip();
        f.clock(1);
        check(f.latch == 0, "Lower bits did not expire on their own deadline");
        f.clock(90000); // Barramento completamente descarregado.
        std::cout << "PPU decay tests passed (per-dot reference, partial drives, expiry, reset, snapshots)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
