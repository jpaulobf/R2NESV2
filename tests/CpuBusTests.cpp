#include "Core/NES.h"
#include <iostream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        NES nes("");
        auto &bus = nes.getBus();
        auto &pad = nes.getJoysticks().controller1;
        pad.setButton(IO::BUTTON_A, true);
        pad.setButton(IO::BUTTON_B, false);
        bus.cpuWrite(0x4016, 1);
        bus.cpuWrite(0x4016, 0);
        bus.cpuWrite(0x0002, 0xE0);
        check(bus.cpuRead(0x4016, true) == 0xE1, "controller peek must preserve upper bus bits");
        check(bus.cpuDataBus == 0xE0, "peek changed bus");
        check(bus.cpuRead(0x4016) == 0xE1, "controller bit and open bus");
        check(bus.cpuRead(0x4016) == 0xE0, "controller peek advanced shift register");

        bus.cpuWrite(0x0002, 0xA0);
        check(bus.cpuRead(0x4017) == 0xA0, "port 2 upper bits must remain open");
        nes.getJoysticks().port2Device = IO::DeviceType::Zapper;
        bus.setZapperTrigger(true);
        bus.cpuWrite(0x0002, 0xE0);
        check((bus.cpuRead(0x4017) & 0xF0) == 0xF0, "zapper trigger and upper bus bits");

        bus.cpuWrite(0x4015, 0);
        bus.cpuWrite(0x0002, 0xA5);
        check((bus.cpuRead(0x4015, true) & 0x20) == 0x20, "APU status bit 5 peek");
        check(bus.cpuDataBus == 0xA5, "APU status peek changed external bus");
        check((bus.cpuRead(0x4015) & 0x20) == 0x20, "APU status bit 5");
        check(bus.cpuDataBus == 0xA5 && bus.cpuRead(0x4000) == 0xA5,
              "APU status read changed external open bus");
        bus.cpuWrite(0x0002, 0x40);
        check((bus.cpuRead(0x4015) & 0x20) == 0, "APU status bit 5 did not clear");

        // LDA $3FFF,X: a leitura descartada de $3F16 alimenta o barramento
        // antes da leitura final de $4016, mesmo quando cruza uma página.
        auto &cpu = nes.getCpu();
        while (!cpu.complete())
            cpu.clock();
        bus.ram->write(0, 0xBD);
        bus.ram->write(1, 0xFF);
        bus.ram->write(2, 0x3F);
        bus.cpuWrite(0x2002, 0xE0);
        cpu.pc = 0;
        cpu.x = 0x17;
        do
        {
            cpu.clock();
        } while (!cpu.complete());
        check((cpu.a & 0xE0) == 0xE0, "indexed dummy read did not reach controller open bus");
        std::cout << "CPU bus tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}