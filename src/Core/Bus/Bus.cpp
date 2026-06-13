#include "Core/Bus/Bus.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include "Core/PPU/PPU.h"
#include "Core/IO/Joysticks.h"
#include "Core/APU/APU.h"
#include <algorithm>

namespace R2NES::Core
{

    Bus::Bus()
    {
        zapperTrigger = false;
    }

    Bus::~Bus()
    {
    }

    void Bus::connectCPU(CPU *pCpu)
    {
        this->cpu = pCpu;
    }

    void Bus::connectRam(RAM *pRam)
    {
        this->ram = pRam;
    }

    void Bus::setCartridge(const std::shared_ptr<Cartridge> &cartridge)
    {
        this->cart = cartridge;
    }

    void Bus::connectPPU(PPU *pPpu)
    {
        this->ppu = pPpu;
    }

    void Bus::connectAPU(APU *pApu)
    {
        this->apu = pApu;
    }

    void Bus::connectJoysticks(IO::Joysticks *joysticks)
    {
        this->joysticks = joysticks;
    }

    void Bus::setZapperTrigger(bool pulled)
    {
        this->zapperTrigger = pulled;
    }

    void Bus::cpuWrite(uint16_t addr, uint8_t data)
    {
        if (cart && cart->cpuWrite(addr, data, systemClockCounter))
        {
            // Cartucho tratou a escrita (Mappers podem interceptar isso)
        }
        else if (addr >= 0x4000 && addr <= 0x4013 || addr == 0x4015 || addr == 0x4017)
        {
            if (apu)
                apu->cpuWrite(addr, data);
        }
        else if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            if (ram)
                ram->write(addr & 0x07FF, data);
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            if (ppu)
                ppu->cpuWrite(addr, data);
        }
        else if (addr == 0x4014)
        {
            // OAM DMA: Inicia a transferência de 256 bytes para a PPU
            uint16_t page = static_cast<uint16_t>(data) << 8;

            if (page < 0x2000 && ram)
            {
                for (uint16_t i = 0; i < 256; i++)
                    ppu->cpuWrite(0x2004, ram->read((page | i) & 0x07FF));
            }
            else
            {
                for (uint16_t i = 0; i < 256; i++)
                    ppu->cpuWrite(0x2004, cpuRead(page | i));
            }

            if (cpu)
                cpu->cycles += 513;
        }
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
        if (cart && cart->cpuRead(addr, data))
        {
            // Cartucho tratou a leitura
        }
        else if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            return ram ? ram->read(addr & 0x07FF) : 0x00;
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF)
        {
            return ppu ? ppu->cpuRead(addr) : 0x00;
        }
        else if (addr == 0x4015)
        {
            return apu ? apu->cpuRead(addr) : 0x00;
        }
        else if (addr == 0x4016)
        {
            return joysticks ? joysticks->controller1.readNextBit() : 0x00;
        }
        else if (addr == 0x4017)
        {
            // O bit 0 continua vindo do controle padrão (entrada serial)
            uint8_t data = (joysticks ? joysticks->controller2.readNextBit() : 0x00);

            // Suporte à Zapper (Pistola) no Port 2

            // Bit 3: Sensor de Luz (0 = Luz detectada, 1 = Nenhuma luz)
            if (ppu && ppu->getZapperLightSense())
                data &= ~0x08; // Limpa o bit 3 (detectado)
            else
                data |= 0x08; // Seta o bit 3 (não detectado)

            // Bit 4: Gatilho (1 = Puxado / 0 = Solto)
            if (zapperTrigger)
                data |= 0x10;
            else
                data &= ~0x10;

            return data;
        }
        return data;
    }

    bool Bus::ppuRead(uint16_t addr, uint8_t &data) const
    {
        if (cart)
            return cart->ppuRead(addr, data, systemClockCounter);
        return false;
    }

    bool Bus::ppuWrite(uint16_t addr, uint8_t data)
    {
        if (cart)
            return cart->ppuWrite(addr, data, systemClockCounter);
        return false;
    }

    MirrorMode Bus::getMirrorMode() const
    {
        if (cart)
            return cart->getMirrorMode();
        return MirrorMode::HORIZONTAL;
    }
}