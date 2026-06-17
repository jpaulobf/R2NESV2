#include "Core/NES.h"
#include <fstream>
#include <iostream>

namespace R2NES::Core
{
  NES::NES()
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

  NES::~NES() {}

  void NES::unload()
  {
    bus.setCartridge(nullptr);
    cartridgeLoaded = false;
    reset();
  }

  void NES::reset()
  {
    // Limpa a RAM interna da CPU (2KB) para garantir um estado limpo (Cold Boot
    // simulation)
    ram.reset();

    apu.reset();
    bus.systemClockCounter = 0;

    if (bus.cart && bus.cart->getMapper())
      bus.cart->getMapper()->reset();

    // CPU deve ser a ÚLTIMA a resetar, para ler os vetores com o Mapper já
    // configurado
    cpu.reset();
    ppu.reset();

    std::cout << "NES: Reset complete. CPU PC at 0x" << std::hex << cpu.pc
              << std::endl;
  }

  void NES::step()
  {
    // Clocka a CPU uma vez. A CPU gerencia seus próprios ciclos internos por
    // instrução.
    cpu.clock();

    // Incrementa o contador de ciclos do sistema (CPU + PPU + APU)
    bus.systemClockCounter++;

    // A PPU roda 3 vezes mais rápido que a CPU.
    // Então, para cada ciclo da CPU, a PPU deve ser clockada 3 vezes.
    ppu.clock();
    ppu.clock();
    ppu.clock();

    // Verifica se a PPU disparou um sinal de NMI (VBlank).
    // A interrupção só deve ser processada quando a CPU terminar a instrução
    // atual para não corromper o estado de execução (Address Latch,
    // registradores, etc).
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
    {
      cpu.irq();

      // Limpa o flag do mapper após consumi-lo (acknowledge edge-triggered).
      // Sem isso, bIRQActive permanece true e cpu.irq() seria chamado
      // repetidamente a cada step até o jogo escrever em $E000
      if (bus.cart)
        bus.cart->clearIrqFlag();
    }
  }

  void NES::insertCartridge(const std::string &path)
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

  bool NES::saveState(const std::string &filename)
  {
    std::ofstream os(filename, std::ios::binary);
    if (!os.is_open())
      return false;

    // 1. Identificador simples para validar o arquivo (Magic Number)
    uint32_t magic = 0x52324E53; // "R2NS"
    os.write(reinterpret_cast<char *>(&magic), sizeof(magic));

    // 2. Salva estado dos componentes
    os.write(reinterpret_cast<char *>(&bus.systemClockCounter),
             sizeof(bus.systemClockCounter));

    cpu.saveState(os);
    ram.saveState(os);
    ppu.saveState(os);
    apu.saveState(os);
    bus.cart->getMapper()->saveState(os);

    os.close();
    return true;
  }

  bool NES::loadState(const std::string &filename)
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
    is.read(reinterpret_cast<char *>(&bus.systemClockCounter),
            sizeof(bus.systemClockCounter));

    cpu.loadState(is);
    ram.loadState(is);
    ppu.loadState(is);
    apu.loadState(is);
    bus.cart->getMapper()->loadState(is);

    is.close();
    return true;
  }
} // namespace R2NES::Core