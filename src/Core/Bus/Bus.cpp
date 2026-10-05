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
        this->cart = cartridge;
    }

    void Bus::connectPPU(PPU *pPpu) { this->ppu = pPpu; }

    void Bus::connectAPU(APU *pApu) { this->apu = pApu; }

    void Bus::connectJoysticks(IO::Joysticks *joysticks)
    {
        this->joysticks = joysticks;
    }

    void Bus::setZapperTrigger(bool pulled) { this->zapperTrigger = pulled; }

    void Bus::cpuWrite(uint16_t addr, uint8_t data)
    {
        // Escrita via CPU: primeiro delega ao cartucho (mappers podem interceptar).
        if (cart && cart->cpuWrite(addr, data, systemClockCounter))
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
            if (joysticks)
            {
                joysticks->controller1.writeStrobe(data);
                joysticks->controller2.writeStrobe(data);
            }
        }
    }

    uint8_t Bus::cpuRead(uint16_t addr, bool readOnly)
    {
        uint8_t data = 0x00;
        // Leitura delegada ao cartucho primeiro (mappers podem prover ROM/RAM mapeada)
        if (cart && cart->cpuRead(addr, data))
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
            return ppu ? ppu->cpuRead(addr) : 0x00;
        }
        // Registrador de status do APU / canais
        else if (addr == 0x4015)
        {
            return apu ? apu->cpuRead(addr) : 0x00;
        }
        // Leitura dos controles / porta 1
        else if (addr == 0x4016)
        {
            return joysticks ? joysticks->controller1.readNextBit() : 0x00;
        }
        // Porta 2: gamepad ou Zapper
        else if (addr == 0x4017)
        {
            uint8_t out = 0x00;

            if (joysticks)
            {
                if (joysticks->port2Device == IO::DeviceType::Gamepad)
                {
                    out = joysticks->controller2.readNextBit();
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

    bool Bus::ppuRead(uint16_t addr, uint8_t &data) const
    {
        // Delegar leitura de VRAM ao cartucho (CHR/RAM mapeada). Retorna true se tratada.
        if (cart)
            return cart->ppuRead(addr, data, systemClockCounter);
        return false;
    }

    bool Bus::ppuReadSprite(uint16_t addr, uint8_t &data) const
    {
        // Leitura específica para fetch de sprites (pode ser tratada pelo cartucho)
        if (cart)
            return cart->ppuReadSprite(addr, data, systemClockCounter);
        return false;
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
