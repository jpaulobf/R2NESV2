#include "Core/NES.h"
#include <fstream>
#include <iostream>

namespace R2NES::Core
{
    NES::NES()
    {
        // Conecta os subsistemas no barramento e entre si.
        // A ordem de conexão importa para garantir que cada componente tenha
        // referência ao barramento antes de ser usado.
        bus.connectCPU(&cpu);
        bus.connectRam(&ram);
        bus.connectJoysticks(&joysticks);
        cpu.connectBus(&bus);
        bus.connectPPU(&ppu);
        ppu.connectBus(&bus);
        bus.connectAPU(&apu);
        apu.connectBus(&bus);

        // Reseta o sistema para um estado inicial conhecido.
        reset();
    }

    NES::~NES() {}

    void NES::unload()
    {
        // Ao remover o cartucho, restaura entrada e limpa referência no barramento.
        joysticks.port2Device = R2NES::Core::IO::DeviceType::Gamepad;
        bus.setCartridge(nullptr);
        cartridgeLoaded = false;
        reset();
    }

    void NES::reset()
    {
        // Limpa a RAM interna da CPU (2KB) para garantir um estado limpo (Cold Boot simulation)
        ram.reset();

        // Reinicia APU e contador de clocks do sistema
        apu.reset();
        bus.systemClockCounter = 0;

        // Se houver um cartucho com mapper, resetamos o mapper também
        if (bus.cart && bus.cart->getMapper())
            bus.cart->getMapper()->reset();

        nmi_delay = 0;

        // CPU deve ser a ÚLTIMA a resetar, para ler os vetores com o Mapper já configurado
        cpu.reset();
        ppu.reset();

        std::cout << "NES: Reset complete. CPU PC at 0x" << std::hex << cpu.pc << std::endl;
    }

    void NES::step()
    {
        // 1) DMA OAM tem prioridade: enquanto um DMA estiver ativo, a CPU fica "suspensa"
        //    e o DMA consome ciclos de sistema para transferir 256 bytes para o PPU.
        if (bus.dma_transfer)
        {
            // Ciclo dummy inicial para alinhar o DMA a um ciclo par
            if (bus.dma_dummy)
            {
                if (bus.systemClockCounter % 2 == 1)
                    bus.dma_dummy = false;
            }
            else
            {
                // Em ciclos pares lê da memória do CPU
                if (bus.systemClockCounter % 2 == 0)
                {
                    bus.dma_data = bus.cpuRead((bus.dma_page << 8) | bus.dma_addr);
                }
                // Em ciclos ímpares escreve no registrador $2004 do PPU
                else
                {
                    ppu.cpuWrite(0x2004, bus.dma_data);
                    bus.dma_addr++;

                    // Após copiar 256 bytes, encerra a transferência
                    if (bus.dma_addr == 0x00)
                    {
                        bus.dma_transfer = false;
                        bus.dma_dummy = true;
                    }
                }
            }
        }
        else
        {
            // CPU executa um ciclo normalmente
            cpu.clock();

            // Modo experimental: quando ativo, damos um ciclo extra à CPU
            // (útil para testes de desempenho / debugging)
            if (cpuOverclock)
            {
                cpu.clock();
            }
        }

        // 2) Avança o relógio do sistema e demais dispositivos
        bus.systemClockCounter++;

        if (bus.cart)
            bus.cart->tick();

        // A PPU roda 3x para cada passo de sistema (CPU)
        ppu.clock();
        ppu.clock();
        ppu.clock();

        // NMI (VBlank) — introduzimos um pequeno delay para evitar NMI hijacking
        if (ppu.nmi)
        {
            ppu.nmi = false;
            nmi_delay = 2; // espera algumas instruções antes de disparar NMI
        }

        if (!bus.dma_transfer)
        {
            if (nmi_delay > 0)
            {
                if (cpu.complete())
                    nmi_delay--;

                if (nmi_delay == 0 && cpu.complete())
                    cpu.nmi();
            }
        }

        // APU avança na mesma cadência da CPU
        apu.step();

        // IRQ pode vir da APU ou do Mapper do cartucho
        bool mapperIrqActive = bus.cart && bus.cart->getIrqFlag();
        bool irqActive = apu.getIrqFlag() || mapperIrqActive;

        // IRQ é reconhecida apenas quando a CPU termina a instrução atual
        if (irqActive && cpu.complete() && cpu.GetFlag(CPU::I) == 0)
        {
            cpu.irq();

            // Limpa o flag do mapper após reconhecimento da IRQ
            if (mapperIrqActive && bus.cart)
                bus.cart->clearIrqFlag();
        }
    }

    void NES::insertCartridge(const std::string &path)
    {
        // Cria e valida o cartucho; em caso de sucesso conecta-o ao barramento.
        std::shared_ptr<Cartridge> newCart = std::make_shared<Cartridge>(path);
        if (newCart->isValid())
        {
            bus.setCartridge(newCart);
            cartridgeLoaded = true;
            std::cout << "Cartridge '" << path << "' loaded successfully. ROM Hash: " << newCart->getRomHash() << std::endl;
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

        return saveState(os);
    }

    bool NES::saveState(std::ostream &os)
    {
        if (!bus.cart || !bus.cart->getMapper())
            return false;

        // Formato simples: Magic number + estado dos componentes, na ordem fixa.
        uint32_t magic = 0x52324E32; // "R2N2" (inclui framebuffer)
        os.write(reinterpret_cast<char *>(&magic), sizeof(magic));

        // Salva contador de clocks do sistema
        os.write(reinterpret_cast<char *>(&bus.systemClockCounter), sizeof(bus.systemClockCounter));

        // Salva os estados dos subsistemas (ordem deve ser preservada ao carregar)
        cpu.saveState(os);
        ram.saveState(os);
        ppu.saveState(os);
        apu.saveState(os);
        bus.cart->getMapper()->saveState(os);

        return os.good();
    }

    bool NES::loadState(const std::string &filename)
    {
        std::ifstream is(filename, std::ios::binary);
        if (!is.is_open())
            return false;

        return loadState(is);
    }

    bool NES::loadState(std::istream &is)
    {
        if (!bus.cart || !bus.cart->getMapper())
            return false;

        uint32_t magic = 0;
        is.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        const bool includesFrameBuffer = magic == 0x52324E32; // "R2N2"
        if (!includesFrameBuffer && magic != 0x52324E53)      // "R2NS" (formato anterior)
        {
            std::cerr << "Error: Invalid SaveState file!" << std::endl;
            return false;
        }

        // Lê o contador de clocks e restaura os subsistemas na mesma ordem do save
        is.read(reinterpret_cast<char *>(&bus.systemClockCounter), sizeof(bus.systemClockCounter));

        cpu.loadState(is);
        ram.loadState(is);
        ppu.loadState(is, includesFrameBuffer);
        apu.loadState(is);
        bus.cart->getMapper()->loadState(is);

        return is.good();
    }

    void NES::setTilesEnabled(bool enabled)
    {
        ppu.setTilesEnabled(enabled);
    }

    void NES::setSpritesEnabled(bool enabled)
    {
        ppu.setSpritesEnabled(enabled);
    }
}