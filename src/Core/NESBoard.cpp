#include "Core/NESBoard.h"
#include <iostream>
#include <fstream>

namespace R2NES::Core
{
    NesBoard::NesBoard()
    {
        bus.connectCPU(&cpu);

        bus.connectRam(&ram);

        bus.connectJoysticks(&joysticks);

        cpu.connectBus(&bus);

        bus.connectPPU(&ppu);

        ppu.connectBus(&bus);

        bus.connectAPU(&apu);

        apu.connectBus(&bus);

        reset();
    }

    NesBoard::~NesBoard() {}

    void NesBoard::unload()
    {
        bus.setCartridge(nullptr);
        cartridgeLoaded = false;
        reset();
    }

    void NesBoard::reset()
    {
        cpu.reset();
        apu.reset();
        systemClockCounter = 0;

        // Teste: Tenta ler os vetores de reset da ROM
        if (bus.cart)
        {
            uint8_t lowByte = bus.cpuRead(0xFFFC);
            uint8_t highByte = bus.cpuRead(0xFFFD);
            uint16_t resetVector = (uint16_t)highByte << 8 | lowByte;
            std::cout << "Cartridge loaded. Reset Vector (0xFFFC-0xFFFD): 0x"
                      << std::hex << (int)lowByte << " 0x" << (int)highByte
                      << " -> 0x" << resetVector << std::endl;

            // Tenta ler o primeiro byte da PRG ROM (endereço 0x8000)
            uint8_t firstPrgByte = bus.cpuRead(0x8000);
            std::cout << "First PRG ROM byte (0x8000): 0x" << std::hex << (int)firstPrgByte << std::endl;
        }
        else
        {
            std::cout << "No cartridge inserted." << std::endl;
        }

        ppu.reset();
    }

    void NesBoard::step()
    {
        // A PPU roda 3 vezes mais rápido que a CPU.
        // Então, para cada ciclo da CPU, a PPU deve ser clockada 3 vezes.
        ppu.clock();
        ppu.clock();
        ppu.clock();

        // Verifica se a PPU disparou um sinal de NMI (VBlank).
        // A interrupção só deve ser processada quando a CPU terminar a instrução atual
        // para não corromper o estado de execução (Address Latch, registradores, etc).
        if (ppu.nmi && cpu.complete())
        {
            ppu.nmi = false;
            cpu.nmi();
        }

        // APU clock (mesma velocidade da CPU)
        apu.step();

        // Propagação do sinal de IRQ (Interrupt Request)
        // O IRQ pode ser disparado pela APU ou pelo Cartucho (Mappers)
        bool irqActive = apu.getIrqFlag();
        if (bus.cart)
            irqActive |= bus.cart->getIrqFlag();

        // A interrupção só é processada quando a CPU termina a instrução atual
        if (irqActive && cpu.complete())
            cpu.irq();

        // Clocka a CPU uma vez. A CPU gerencia seus próprios ciclos internos por instrução.
        cpu.clock();
        systemClockCounter++;
    }

    void NesBoard::insertCartridge(const std::string &path)
    {
        std::shared_ptr<Cartridge> newCart = std::make_shared<Cartridge>(path);
        if (newCart->isValid())
        {
            bus.setCartridge(newCart);
            cartridgeLoaded = true;
            std::cout << "Cartridge '" << path << "' loaded successfully." << std::endl;
        }
        else
        {
            cartridgeLoaded = false;
            std::cerr << "Failed to load cartridge '" << path << "'." << std::endl;
        }
    }

    bool NesBoard::saveState(const std::string &filename)
    {
        std::ofstream os(filename, std::ios::binary);
        if (!os.is_open())
            return false;

        // 1. Identificador simples para validar o arquivo (Magic Number)
        uint32_t magic = 0x52324E53; // "R2NS"
        os.write(reinterpret_cast<char *>(&magic), sizeof(magic));

        // 2. Salva estado dos componentes
        os.write(reinterpret_cast<char *>(&systemClockCounter), sizeof(systemClockCounter));

        cpu.saveState(os);
        // ram.saveState(os);
        // ppu.saveState(os);
        // apu.saveState(os);
        // bus.cart->getMapper()->saveState(os);

        os.close();
        return true;
    }

    bool NesBoard::loadState(const std::string &filename)
    {
        std::ifstream is(filename, std::ios::binary);
        if (!is.is_open())
            return false;

        uint32_t magic = 0;
        is.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (magic != 0x52324E53)
        {
            std::cerr << "Error: Invalid SaveState file!" << std::endl;
            return false;
        }

        // 2. Lê estado dos componentes (Exatamente na mesma ordem do save)
        is.read(reinterpret_cast<char *>(&systemClockCounter), sizeof(systemClockCounter));

        cpu.loadState(is);
        // ram.loadState(is);
        // ppu.loadState(is);
        // apu.loadState(is);
        // bus.cart->getMapper()->loadState(is);

        is.close();
        return true;
    }
}