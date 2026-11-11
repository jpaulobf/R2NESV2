#include "Core/NES.h"
#include <algorithm>
#include <cmath>
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
        dmcDmaPhase = 0;
        dmcHaltAddress = 0;
        oamReadAvailable = false;
        bus.resetControllerStrobe();
        palPhase = 0;
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

    void NES::setPAL(bool enabled)
    {
        if (pal == enabled)
            return;
        pal = enabled;
        ppu.setPAL(enabled);
        apu.setPAL(enabled);
        reset();
    }
    void NES::setCPUOverclock(bool enabled, float multiplier)
    {
        if (!std::isfinite(multiplier))
            multiplier = 1.0f;
        cpuMultiplierPercent = enabled ? static_cast<unsigned>(std::lround(std::clamp(multiplier, 1.0f, 2.0f) * 100.0f)) : 100;
    }

    void NES::step(bool honorCPUOverclock)
    {
        if (pal)
            stepPAL(honorCPUOverclock);
        else
            stepNTSC(honorCPUOverclock);
    }
    void NES::stepNTSC(bool honorCPUOverclock) { stepRegion<false>(honorCPUOverclock); }
    void NES::stepPAL(bool honorCPUOverclock) { stepRegion<true>(honorCPUOverclock); }
    template <bool Pal>
    void NES::stepRegion(bool honorCPUOverclock)
    {
        if (bus.cheats.hasActiveRamPatches())
            bus.cheats.applyRam(bus);
        // RDY só para a CPU em leituras. Uma escrita RMW em $4014 ainda
        // deve completar a segunda escrita, que seleciona a página da DMA.
        if (bus.dma_transfer && !(bus.dma_dummy && cpu.nextCycleIsWrite()))
        {
            ++bus.cpuCycleCounter;
            const bool dmcGet = clockDmcDma(true);
            if (bus.dma_dummy)
            {
                // Ciclo dummy inicial para alinhar o DMA a um ciclo par.
                if (bus.systemClockCounter % 2 == 1)
                    bus.dma_dummy = false;
            }
            else
            {
                // Em ciclos pares lê da memória do CPU
                if (bus.systemClockCounter % 2 == 0 && !dmcGet)
                {
                    bus.dma_data = bus.readOamDmaSource((bus.dma_page << 8) | bus.dma_addr,
                                                        cpu.nextReadAddress());
                    oamReadAvailable = true;
                }
                // Em ciclos ímpares escreve no registrador $2004 do PPU
                else if ((bus.systemClockCounter & 1) && oamReadAvailable)
                {
                    ppu.cpuWrite(0x2004, bus.dma_data);
                    bus.cpuDataBus = bus.dma_data;
                    oamReadAvailable = false;
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
            // A fase deriva do clock salvo, mantendo rewind e savestates determinísticos.
            const unsigned extra = honorCPUOverclock ? cpuMultiplierPercent - 100 : 0;
            const unsigned phase = (bus.systemClockCounter % 100) * extra % 100;
            const uint8_t cpuClocksThisStep = 1 + (phase + extra >= 100 ? 1 : 0);
            for (uint8_t i = 0; i < cpuClocksThisStep; i++)
            {
                ++bus.cpuCycleCounter;
                // O DMA de DMC usa o clock do sistema, não os ciclos extras da CPU.
                if (apu.consumeDmcDmaStallCycle() || clockDmcDma(false))
                    break;

                serviceIRQ();
                cpu.clock();

                // A DMA suspende a CPU. Não consome o clock extra se esta
                // chamada acabou de iniciar uma DMA de OAM.
                if (bus.dma_transfer)
                    break;
            }
        }

        // Amostra a saída de $4016 uma vez a cada dois ciclos do sistema.
        if ((bus.systemClockCounter & 1) == 0)
            bus.clockControllerStrobe();

        // 2) Avança o relógio do sistema e demais dispositivos
        bus.systemClockCounter++;

        if (bus.cart)
            bus.cart->tick();

        // A PPU roda 3x para cada passo de sistema (CPU)
        ppu.clock();
        ppu.clock();
        ppu.clock();
        if constexpr (Pal)
        {
            if (++palPhase == 5)
            {
                palPhase = 0;
                ppu.clock();
            }
        }

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

    bool NES::clockDmcDma(bool oamActive)
    {
        // A 2A07 tem regras diferentes de parada; mantém o comportamento
        // anterior até existir uma implementação e testes específicos PAL.
        if (pal)
        {
            if (oamActive || !apu.hasDmcDmaRequest() || cpu.nextCycleIsWrite())
                return false;
            const uint8_t value = bus.cpuRead(apu.getDmcDmaAddress());
            apu.completeDmcDma(value, (bus.systemClockCounter & 1) ? 4 : 3);
            return true;
        }
        if (dmcDmaPhase == 0)
        {
            if (!apu.isDmcDmaReady())
                return false;
            if (apu.isDmcDmaAbortPending())
            {
                if (oamActive || cpu.nextCycleIsWrite())
                {
                    apu.finishDmcDmaAbort();
                    return false;
                }
                dmcHaltAddress = cpu.nextReadAddress();
                bus.cpuRead(dmcHaltAddress);
                apu.finishDmcDmaAbort();
                return true;
            }
            if (!oamActive && cpu.nextCycleIsWrite())
                return false;
            if (!oamActive)
            {
                dmcHaltAddress = cpu.nextReadAddress();
                cpu.notifyDmcHalt();
                bus.cpuRead(dmcHaltAddress);
            }
            dmcDmaPhase = 1;
            return !oamActive;
        }
        if (dmcDmaPhase == 1)
        {
            if (!oamActive)
                bus.cpuRead(dmcHaltAddress);
            dmcDmaPhase = 2;
            return !oamActive;
        }
        if (bus.systemClockCounter & 1)
        {
            if (!oamActive)
                bus.cpuRead(dmcHaltAddress);
            return !oamActive;
        }
        if (apu.hasDmcDmaRequest())
        {
            const uint16_t sampleAddress = apu.getDmcDmaAddress();
            const uint8_t sample = bus.cpuRead(sampleAddress);
            // Com a CPU parada em $4000-$401F, o decodificador interno
            // permanece ativo durante o get DMC; os cinco bits baixos do
            // endereço da amostra selecionam o registrador que responde.
            if (!oamActive && (dmcHaltAddress & 0xFFE0) == 0x4000)
            {
                const uint16_t reg = 0x4000 | (sampleAddress & 0x1F);
                if (reg >= 0x4015 && reg <= 0x4017)
                    bus.clockDmcRegisterConflict(reg);
            }
            apu.completeDmcDma(sample, 0);
        }
        dmcDmaPhase = 0;
        return true;
    }

    void NES::serviceIRQ()
    {
        // A CPU faz o polling no ciclo correto da instrução. Aceitar uma IRQ
        // não reconhece automaticamente a fonte: o software deve fazê-lo.
        cpu.setIrqLine(apu.getIrqFlag() || (bus.cart && bus.cart->getIrqFlag()));
    }

    bool NES::insertCartridge(const std::string &path)
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
            return true;
        }
        else
        {
            std::cerr << "Failed to load cartridge '" << path << "'." << std::endl;
            return false;
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
        uint32_t magic = pal ? 0x52324E56 : 0x52324E55; // R2NV/R2NU: agendamento da carga DMC.
        os.write(reinterpret_cast<char *>(&magic), sizeof(magic));
        const auto hash = bus.cart->getRomHash();
        if (hash.size() != 8)
            return false;
        os.write(hash.data(), 8);
        const uint16_t mapperId = bus.cart->getMapperId();
        os.put(mapperId & 255);
        os.put(mapperId >> 8);

        if (pal)
            os.put(palPhase);

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
        os.put(bus.getControllerStrobeOutput());
        os.put(joysticks.controller1.getShiftState());
        os.put(joysticks.controller1.getStrobeState());
        os.put(joysticks.controller2.getShiftState());
        os.put(joysticks.controller2.getStrobeState());
        os.put(dmcDmaPhase);
        os.put(dmcHaltAddress & 0xff);
        os.put(dmcHaltAddress >> 8);
        os.put(oamReadAvailable);
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

        // The serialized state deliberately excludes host queues and transient
        // component fields. Keep them when a failed load is rolled back.
        const bool previousIrqLine = cpu.isIrqLineAsserted();
        auto previousPpu = std::make_unique<PPU>(ppu);
        auto previousApu = std::make_unique<APU>(apu);
        // Um erro em qualquer componente restaura o snapshot anterior do console.
        std::stringstream previous;
        if (!saveState(previous))
            return false;
        const auto restore = [&](std::istream &input) -> bool
        {
            uint32_t magic = 0;
            input.read(reinterpret_cast<char *>(&magic), sizeof(magic));
            if (!input || magic != (pal ? 0x52324E56 : 0x52324E55))
                return false;
            // Confere a identidade antes de alterar qualquer componente.
            char hash[8] = {};
            input.read(hash, sizeof(hash));
            const int mapperLow = input.get(), mapperHigh = input.get();
            if (!input || std::string(hash, sizeof(hash)) != bus.cart->getRomHash() ||
                (mapperLow | (mapperHigh << 8)) != bus.cart->getMapperId())
                return false;
            if (pal)
            {
                const int phase = input.get();
                if (phase < 0 || phase >= 5)
                    return false;
                palPhase = static_cast<uint8_t>(phase);
            }
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
            ppu.loadState(input);
            if (!input)
                return false;
            apu.loadState(input);
            if (!input)
                return false;
            {
                const int output = input.get(), shift1 = input.get(), strobe1 = input.get();
                const int shift2 = input.get(), strobe2 = input.get();
                if (!input || output > 1 || strobe1 > 1 || strobe2 > 1)
                    return false;
                bus.setControllerStrobeOutput(output != 0);
                joysticks.controller1.restoreLatch(static_cast<uint8_t>(shift1), strobe1 != 0);
                joysticks.controller2.restoreLatch(static_cast<uint8_t>(shift2), strobe2 != 0);
            }
            {
                const int phase = input.get(), low = input.get(), high = input.get(), oamReady = input.get();
                if (!input || phase > 2 || oamReady > 1)
                    return false;
                dmcDmaPhase = static_cast<uint8_t>(phase);
                dmcHaltAddress = static_cast<uint16_t>(low | (high << 8));
                oamReadAvailable = oamReady != 0;
            }
            bus.cart->getMapper()->loadState(input);
            if (!input)
                return false;
            bus.cart->loadState(input);
            return input.good();
        };
        if (restore(is))
            return true;
        restore(previous);
        cpu.setIrqLine(previousIrqLine);
        ppu = std::move(*previousPpu);
        apu = std::move(*previousApu);
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