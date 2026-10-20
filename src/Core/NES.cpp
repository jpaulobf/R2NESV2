#include "Core/NES.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace R2NES::Core
{
    NES::NES(std::filesystem::path directory) : batteryDirectory(std::move(directory))
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

    NES::~NES()
    {
        try
        {
            flushBattery();
        }
        catch (const std::exception &error)
        {
            std::cerr << "Battery save failed: " << error.what() << '\n';
        }
    }

    bool NES::flushBattery()
    {
        return !bus.cart || bus.cart->flushBattery();
    }

    void NES::unload()
    {
        flushBattery();
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
        bus.cpuCycleCounter = 0;
        bus.cpuDataBus = 0;
        bus.dma_page = 0;
        bus.dma_addr = 0;
        bus.dma_data = 0;
        bus.dma_transfer = false;
        bus.dma_dummy = true;

        // Se houver um cartucho com mapper, resetamos o mapper também
        if (bus.cart && bus.cart->getMapper())
            bus.cart->getMapper()->reset();

        ppu.reset();
        // O mapper já está pronto; o vetor será lido pela CPU, ciclo a ciclo.
        cpu.reset();

        std::cout << "NES: Reset sequence started (7 CPU cycles)." << std::endl;
    }

    void NES::step(bool honorCPUOverclock)
    {
        if (bus.cheats.hasActiveRamPatches())
            bus.cheats.applyRam(bus);
        // 1) DMA OAM tem prioridade: enquanto um DMA estiver ativo, a CPU fica "suspensa"
        //    e o DMA consome ciclos de sistema para transferir 256 bytes para o PPU.
        if (bus.dma_transfer)
        {
            ++bus.cpuCycleCounter;
            // Mantém o OAM DMA contínuo. A arbitragem DMC/OAM ainda não é
            // modelada ciclo a ciclo; iniciar um DMC DMA aqui pode deslocar
            // os slots de leitura/escrita e corromper os sprites copiados.
            if (bus.dma_dummy)
            {
                // Ciclo dummy inicial para alinhar o DMA a um ciclo par.
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
            // O overclock acelera apenas a CPU; PPU/APU e o relógio do sistema
            // continuam avançando na cadência normal.
            const uint8_t cpuClocksThisStep = (cpuOverclock && honorCPUOverclock) ? 2 : 1;
            for (uint8_t i = 0; i < cpuClocksThisStep; i++)
            {
                ++bus.cpuCycleCounter;
                if (apu.consumeDmcDmaStallCycle())
                    continue;

                // A leitura do DMC ocupa este ciclo da CPU e agenda os ciclos
                // de parada restantes dentro do próprio APU.
                if (startDmcDma())
                    continue;

                serviceIRQ();
                cpu.clock();

                // A DMA suspende a CPU. Não consome o clock extra se esta
                // chamada acabou de iniciar uma DMA de OAM.
                if (bus.dma_transfer)
                    break;
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

        // A borda de NMI é entregue à CPU; a CPU a aceita no próximo limite
        // de instrução e executa a entrada da interrupção nos ciclos reais.
        if (ppu.nmi)
        {
            ppu.nmi = false;
            cpu.nmi();
        }

        // APU avança na mesma cadência da CPU
        apu.step();

        serviceIRQ();
    }

    bool NES::startDmcDma()
    {
        if (!apu.hasDmcDmaRequest())
            return false;

        const uint16_t address = apu.getDmcDmaAddress();
        const uint8_t value = bus.cpuRead(address);
        const uint8_t totalStallCycles = (bus.systemClockCounter & 1) ? 4 : 3;
        apu.completeDmcDma(value, totalStallCycles);
        return true;
    }

    void NES::serviceIRQ()
    {
        // A CPU faz o polling no ciclo correto da instrução. Aceitar uma IRQ
        // não reconhece automaticamente a fonte: o software deve fazê-lo.
        cpu.setIrqLine(apu.getIrqFlag() || (bus.cart && bus.cart->getIrqFlag()));
    }

    void NES::insertCartridge(const std::string &path)
    {
        // Cria e valida o cartucho; em caso de sucesso conecta-o ao barramento.
        std::shared_ptr<Cartridge> newCart = std::make_shared<Cartridge>(path);
        if (newCart->isValid())
        {
            // Salva o anterior antes de ler o novo, inclusive ao recarregar a mesma ROM.
            flushBattery();
            newCart->loadBattery(batteryDirectory);
            bus.setCartridge(newCart);
            bus.cheats.load(cheatDirectory / (newCart->getRomHash() + ".cht"));
            cartridgeLoaded = true;
            std::cout << "Cartridge '" << path << "' loaded successfully. ROM Hash: " << newCart->getRomHash() << std::endl;
        }
        else
        {
            bus.cheats.clear();
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
        uint32_t magic = 0x52324E4B; // "R2NK": identidade do cartucho e transferência DMA completa.
        os.write(reinterpret_cast<char *>(&magic), sizeof(magic));
        const auto hash = bus.cart->getRomHash();
        if (hash.size() != 8)
            return false;
        os.write(hash.data(), 8);
        const uint16_t mapperId = bus.cart->getMapperId();
        os.put(mapperId & 255);
        os.put(mapperId >> 8);

        // Salva contador de clocks do sistema
        os.write(reinterpret_cast<char *>(&bus.systemClockCounter), sizeof(bus.systemClockCounter));
        os.write(reinterpret_cast<char *>(&bus.cpuCycleCounter), sizeof(bus.cpuCycleCounter));
        os.write(reinterpret_cast<char *>(&bus.cpuDataBus), sizeof(bus.cpuDataBus));
        os.put(bus.dma_page);
        os.put(bus.dma_addr);
        os.put(bus.dma_data);
        os.put(bus.dma_transfer);
        os.put(bus.dma_dummy);

        // Salva os estados dos subsistemas (ordem deve ser preservada ao carregar)
        cpu.saveState(os);
        ram.saveState(os);
        ppu.saveState(os);
        apu.saveState(os);
        bus.cart->getMapper()->saveState(os);
        bus.cart->saveState(os);

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

        // Um erro em qualquer componente restaura o snapshot anterior do console.
        std::stringstream previous;
        if (!saveState(previous))
            return false;
        const auto restore = [&](std::istream &input) -> bool
        {
            uint32_t magic = 0;
            input.read(reinterpret_cast<char *>(&magic), sizeof(magic));
            // Estados anteriores não identificam estas configurações de memória.
            if (!input || magic != 0x52324E4B)
                return false;
            // Confere a identidade antes de alterar qualquer componente.
            char hash[8] = {};
            input.read(hash, sizeof(hash));
            const int mapperLow = input.get(), mapperHigh = input.get();
            if (!input || std::string(hash, sizeof(hash)) != bus.cart->getRomHash() ||
                (mapperLow | (mapperHigh << 8)) != bus.cart->getMapperId())
                return false;
            input.read(reinterpret_cast<char *>(&bus.systemClockCounter), sizeof(bus.systemClockCounter));
            input.read(reinterpret_cast<char *>(&bus.cpuCycleCounter), sizeof(bus.cpuCycleCounter));
            input.read(reinterpret_cast<char *>(&bus.cpuDataBus), sizeof(bus.cpuDataBus));
            bus.dma_page = uint8_t(input.get());
            bus.dma_addr = uint8_t(input.get());
            bus.dma_data = uint8_t(input.get());
            const int dmaActive = input.get(), dmaDummy = input.get();
            if ((dmaActive != 0 && dmaActive != 1) || (dmaDummy != 0 && dmaDummy != 1))
                return false;
            bus.dma_transfer = dmaActive != 0;
            bus.dma_dummy = dmaDummy != 0;
            if (!input)
                return false;
            cpu.loadState(input);
            if (!input)
                return false;
            ram.loadState(input);
            if (!input)
                return false;
            ppu.loadState(input, true);
            if (!input)
                return false;
            apu.loadState(input);
            if (!input)
                return false;
            bus.cart->getMapper()->loadState(input);
            if (!input)
                return false;
            bus.cart->loadState(input);
            return input.good();
        };
        if (restore(is))
            return true;
        restore(previous);
        is.setstate(std::ios::failbit);
        return false;
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
