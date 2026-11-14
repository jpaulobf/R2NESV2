#include "Core/Bus/Bus.h"
#include "Core/APU/APU.h"
#include "Core/Cartridge/Cartridge.h"
#include "Core/IO/Joysticks.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/PPU/PPU.h"
#include <algorithm>

namespace R2NES::Core
{

    Bus::Bus()
    {
        zapperTrigger = false;
        dma_page = 0x00;
        dma_addr = 0x00;
        dma_data = 0x00;
        dma_transfer = false;
        dma_dummy = true;
    }

    Bus::~Bus() {}

    void Bus::connectCPU(CPU *pCpu) { this->cpu = pCpu; }

    void Bus::connectRam(RAM *pRam) { this->ram = pRam; }

    void Bus::setCartridge(const std::shared_ptr<Cartridge> &cartridge)
    {
        cheats.clear();
        this->cart = cartridge;
    }

    void Bus::connectPPU(PPU *pPpu) { this->ppu = pPpu; }

    void Bus::connectAPU(APU *pApu) { this->apu = pApu; }

    void Bus::connectJoysticks(IO::Joysticks *joysticks)
    {
        this->joysticks = joysticks;
    }

    void Bus::clockControllerStrobe()
    {
        if (joysticks)
        {
            joysticks->controller1.writeStrobe(controllerStrobeOutput);
            joysticks->controller2.writeStrobe(controllerStrobeOutput);
        }
    }

    void Bus::resetControllerStrobe()
    {
        controllerStrobeOutput = false;
        clockControllerStrobe();
    }

    void Bus::setZapperTrigger(bool pulled) { this->zapperTrigger = pulled; }

    void Bus::cpuWrite(uint16_t addr, uint8_t data)
    {
        if (probingCpuAccess)
        {
            probedCpuWrite = true;
            return;
        }
        cpuDataBus = data;
        cpuInternalBusBit5 = data & 0x20;
        // Escrita via CPU: primeiro delega ao cartucho (mappers podem interceptar).
        if (cart && cart->cpuWrite(addr, data, systemClockCounter, cpuCycleCounter))
        {
            // Cartucho tratou a escrita.
        }
        // Regiões mapeadas para APU (0x4000-0x4017 inclusive)
        else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015 || addr == 0x4017)
        {
            if (apu)
                apu->cpuWrite(addr, data);
        }
        // RAM interna (mirrored a cada 0x800)
        else if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            if (ram)
                ram->write(addr & 0x07FF, data);
        }
        // Registradores da PPU (mirrored 0x2000-0x3FFF)
        else if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            if (ppu)
                ppu->cpuWrite(addr, data);
        }
        // OAM DMA (0x4014): inicializa transferência de 256 bytes da página indicada
        else if (addr == 0x4014)
        {
            dma_page = data;
            dma_addr = 0x00;
            dma_transfer = true;
            dma_dummy = true;
        }
        // Strobe dos controles (0x4016): re-sincroniza os shift-registers dos gamepads
        else if (addr == 0x4016)
        {
            controllerStrobeOutput = (data & 1) != 0;
        }
    }

    uint8_t Bus::cpuRead(uint16_t addr, bool readOnly)
    {
        if (probingCpuAccess)
        {
            probedCpuAddress = addr;
            return 0;
        }
        bool internalApuRead = false;
        const uint8_t original = readCpuDevice(addr, readOnly, internalApuRead);
        const uint8_t value = cheats.filterRead(addr, original);
        if (!readOnly)
        {
            cpuInternalBusBit5 = value & 0x20;
            // $4015 é interno à CPU: a leitura não conduz o barramento externo.
            if (!internalApuRead)
                cpuDataBus = value;
            if (cart)
                cart->cpuReadOccurred(addr, original);
        }
        return value;
    }

    uint8_t Bus::readOamDmaSource(uint16_t addr, uint16_t haltedCpuAddress)
    {
        // O decodificador dos registradores internos vê os bits altos do
        // endereço da CPU parada e os cinco bits baixos da fonte da DMA.
        const bool internalActive = (haltedCpuAddress & 0xFFE0) == 0x4000;
        const uint16_t reg = 0x4000 | (addr & 0x001F);
        if (internalActive && reg >= 0x4015 && reg <= 0x4017)
        {
            uint8_t external = cpuDataBus;
            bool externalDriven = cart && cart->cpuRead(addr, external, false);
            if (!externalDriven && addr < 0x2000)
            {
                external = ram ? ram->read(addr & 0x07FF) : 0;
                externalDriven = true;
            }
            else if (!externalDriven && addr < 0x4000)
            {
                external = ppu ? ppu->cpuRead(addr, false) : 0;
                externalDriven = true;
            }
            cpuDataBus = external;

            uint8_t original = external;
            if (reg == 0x4015)
            {
                // O status dirige o barramento interno, preservando o bit 5
                // do barramento externo, mas não altera a trava externa.
                original = static_cast<uint8_t>((apu ? apu->cpuRead(reg) : 0) | (external & 0x20));
            }
            else
            {
                // Os controles ainda recebem o pulso de leitura. Quando a
                // fonte da DMA dirige o barramento, seu dado prevalece.
                bool internalApuRead = false;
                const uint8_t controller = readCpuDevice(reg, false, internalApuRead, true);
                if (!externalDriven)
                    original = controller;
            }
            const uint8_t value = cheats.filterRead(addr, original);
            if (reg != 0x4015)
                cpuDataBus = value;
            if (cart)
                cart->cpuReadOccurred(addr, original);
            return value;
        }

        // Fora da faixa ativa, $4000-$401F não ativa APU nem controles.
        // A região de expansão $4020-$40FF continua acessível ao cartucho.
        if ((addr & 0xFFE0) == 0x4000)
        {
            uint8_t original = cpuDataBus;
            if (cart && cart->cpuRead(addr, original, false))
                cpuDataBus = cheats.filterRead(addr, original);
            if (cart)
                cart->cpuReadOccurred(addr, original);
            return cpuDataBus;
        }
        return cpuRead(addr);
    }

    void Bus::clockDmcRegisterConflict(uint16_t addr)
    {
        bool internalApuRead = false;
        const uint8_t value = readCpuDevice(addr, false, internalApuRead, true);
        if (!internalApuRead)
            cpuDataBus = value;
    }

    uint8_t Bus::readCpuDevice(uint16_t addr, bool readOnly, bool &internalApuRead, bool skipCartridge)
    {
        uint8_t data = cpuDataBus;
        // Leitura delegada ao cartucho primeiro (mappers podem prover ROM/RAM mapeada)
        if (!skipCartridge && cart && cart->cpuRead(addr, data, readOnly))
        {
            // Cartucho supriu o dado
            return data;
        }
        // Leitura da RAM interna
        else if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            return ram ? ram->read(addr & 0x07FF) : 0x00;
        }
        // Registradores da PPU
        else if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            return ppu ? ppu->cpuRead(addr, readOnly) : 0x00;
        }
        // Registrador de status do APU / canais
        else if (addr == 0x4015)
        {
            internalApuRead = true;
            return static_cast<uint8_t>((apu ? apu->cpuRead(addr, readOnly) : 0x00) | cpuInternalBusBit5);
        }
        // Leitura dos controles / porta 1
        else if (addr == 0x4016)
        {
            const uint8_t bit = joysticks ? (readOnly ? joysticks->controller1.peekNextBit()
                                                      : joysticks->controller1.readNextBit())
                                          : 0;
            return static_cast<uint8_t>((cpuDataBus & 0xE0) | bit);
        }
        // Porta 2: gamepad ou Zapper
        else if (addr == 0x4017)
        {
            uint8_t out = cpuDataBus & 0xE0;

            if (joysticks)
            {
                if (joysticks->port2Device == IO::DeviceType::Gamepad)
                {
                    out |= readOnly ? joysticks->controller2.peekNextBit()
                                    : joysticks->controller2.readNextBit();
                }
                else if (joysticks->port2Device == IO::DeviceType::Zapper)
                {
                    // Para a Zapper: bit 3 = sensor de luz (0 quando há luz),
                    // bit 4 = gatilho (1 quando puxado).
                    if (ppu && ppu->getZapperLightSense())
                        out &= ~0x08; // detectado -> limpa o bit 3
                    else
                        out |= 0x08; // não detectado -> seta o bit 3

                    if (zapperTrigger)
                        out |= 0x10; // gatilho puxado
                    else
                        out &= ~0x10; // gatilho solto
                }
            }

            return out;
        }

        return data;
    }

    bool Bus::ppuRead(uint16_t addr, uint8_t &data, PpuReadContext context) const
    {
        // Delegar leitura de VRAM ao cartucho (CHR/RAM mapeada). Retorna true se tratada.
        if (cart)
            return cart->ppuRead(addr, data, systemClockCounter, context);
        return false;
    }

    void Bus::ppuReadOccurred(uint16_t addr)
    {
        if (cart)
            cart->ppuReadOccurred(addr);
    }

    void Bus::ppuBackgroundTile(int16_t scanline, uint8_t column)
    {
        if (cart && cart->getMapper())
            cart->getMapper()->onPpuBackgroundTile(scanline, column);
    }

    bool Bus::ppuReadSprite(uint16_t addr, uint8_t &data) const
    {
        // Leitura específica para fetch de sprites (pode ser tratada pelo cartucho)
        if (cart)
            return cart->ppuReadSprite(addr, data, systemClockCounter);
        return false;
    }

    bool Bus::ppuPeek(uint16_t addr, uint8_t &data) const
    {
        // O visualizador não deve acionar os latches de CHR do MMC2.
        return cart && cart->ppuPeek(addr, data, systemClockCounter);
    }

    void Bus::ppuAddressUpdated(uint16_t addr)
    {
        if (cart)
            cart->ppuAddressUpdated(addr, systemClockCounter);
    }

    void Bus::ppuScanlineStart()
    {
        // Notifica o cartucho que uma nova scanline começou (mappers com IRQ por scanline)
        if (cart)
            cart->ppuScanlineStart();
    }

    bool Bus::ppuWrite(uint16_t addr, uint8_t data)
    {
        // Escrita de VRAM/CHR via cartucho, quando aplicável.
        if (cart)
            return cart->ppuWrite(addr, data, systemClockCounter);
        return false;
    }

    MirrorMode Bus::getMirrorMode() const
    {
        // Retorna o modo de espelhamento definido pelo cartucho, ou horizontal por padrão.
        if (cart)
            return cart->getMirrorMode();
        return MirrorMode::HORIZONTAL;
    }
}