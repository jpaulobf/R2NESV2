#include "Core/NESBoard.h"
#include <iostream>

namespace R2NES::Core
{
    NesBoard::NesBoard()
    {
        // Conecta a RAM ao Bus
        bus.connectRam(&ram);

        // Conecta a CPU ao Bus
        cpu.connectBus(&bus);

        bus.connectPPU(&ppu);

        ppu.connectBus(&bus);

        reset(); // Reseta o sistema para um estado inicial
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

        // Clocka a CPU uma vez. A CPU gerencia seus próprios ciclos internos por instrução.
        cpu.clock();
        systemClockCounter++;
    }

    void NesBoard::insertCartridge(const std::string &path)
    {
        std::shared_ptr<Cartridge> newCart = std::make_shared<Cartridge>(path);
        if (newCart->isValid())
        { // Usando o método público isValid()
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
}