#include "Core/Bus/Bus.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include "Core/PPU/PPU.h"
#include <algorithm>

namespace R2NES::Core {

    Bus::Bus() {
    }

    Bus::~Bus() {
    }

    void Bus::connectRam(RAM* pRam) {
        this->ram = pRam;
    }

    void Bus::setCartridge(const std::shared_ptr<Cartridge>& cartridge) {
        this->cart = cartridge;
    }

    void Bus::connectPPU(PPU* pPpu) {
        this->ppu = pPpu;
    }

    void Bus::cpuWrite(uint16_t addr, uint8_t data) {
        if (cart && cart->cpuWrite(addr, data)) {
            // Cartucho tratou a escrita (Mappers podem interceptar isso)
        }
        else if (addr >= 0x0000 && addr <= 0x1FFF) {
            if (ram) ram->write(addr & 0x07FF, data);
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            if (ppu) ppu->cpuWrite(addr, data);
        }
        else if (addr == 0x4014) {
            // OAM DMA: Inicia a transferência de 256 bytes para a PPU
            uint16_t page = static_cast<uint16_t>(data) << 8;
            for (uint16_t i = 0; i < 256; i++) {
                uint8_t value = cpuRead(page | i);
                if (ppu) ppu->cpuWrite(0x2004, value);
            }

            // Nota: Em uma implementação de "ciclo exato", a CPU deveria ser
            // suspensa por aproximadamente 513 ciclos aqui.
        }
    }

    uint8_t Bus::cpuRead(uint16_t addr, bool readOnly) {
        uint8_t data = 0x00;
        if (cart && cart->cpuRead(addr, data)) {
            // Cartucho tratou a leitura
        }
        else if (addr >= 0x0000 && addr <= 0x1FFF) {
            return ram ? ram->read(addr & 0x07FF) : 0x00;
        }
        else if (addr >= 0x2000 && addr <= 0x3FFF) {
            return ppu ? ppu->cpuRead(addr) : 0x00;
        }
        return data;
    }

    bool Bus::ppuRead(uint16_t addr, uint8_t &data) const {
        if (cart) return cart->ppuRead(addr, data);
        return false;
    }

    bool Bus::ppuWrite(uint16_t addr, uint8_t data) {
        if (cart) return cart->ppuWrite(addr, data);
        return false;
    }

    MirrorMode Bus::getMirrorMode() const {
        if (cart) return cart->getMirrorMode();
        return MirrorMode::HORIZONTAL;
    }
}