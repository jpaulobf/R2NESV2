#include "Core/Bus/Bus.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
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

    void Bus::cpuWrite(uint16_t addr, uint8_t data) {
        if (cart && cart->cpuWrite(addr, data)) {
            // Cartucho tratou a escrita (Mappers podem interceptar isso)
        }
        else if (addr >= 0x0000 && addr <= 0x1FFF) {
            if (ram) ram->write(addr & 0x07FF, data);
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
        return data;
    }

}