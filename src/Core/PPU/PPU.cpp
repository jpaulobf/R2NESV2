#include "Core/PPU/PPU.h"
#include "Core/Cartridge/Cartridge.h"
#include <algorithm>

namespace R2NES::Core
{
    PPU::PPU()
    {
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);
    }

    PPU::~PPU()
    {
    }

    void PPU::connectCartridge(const std::shared_ptr<Cartridge> &cartridge)
    {
        this->cart = cartridge;
    }

    uint8_t PPU::cpuRead(uint16_t addr)
    {
        // TODO: Implementar leitura de registradores como PPUSTATUS ($2002)
        return 0x00;
    }

    void PPU::cpuWrite(uint16_t addr, uint8_t data)
    {
        switch (addr)
        {
        case 0x0006: // PPUADDR ($2006)
            // Escrita dupla: primeiro MSB, depois LSB
            if (addressLatch == 0)
            {
                ppuAddress = (ppuAddress & 0x00FF) | (static_cast<uint16_t>(data & 0x3F) << 8);
                addressLatch = 1;
            }
            else
            {
                ppuAddress = (ppuAddress & 0xFF00) | data;
                addressLatch = 0;
            }
            break;

        case 0x0007: // PPUDATA ($2007)
            ppuWrite(ppuAddress, data);
            // Incremento automático de endereço (TODO: verificar bit no PPUCTRL)
            ppuAddress += 1;
            break;
        }
    }

    uint8_t PPU::ppuRead(uint16_t addr)
    {
        addr &= 0x3FFF;
        // Lógica de roteamento: Pattern Tables -> Name Tables -> Paletas
        return 0x00;
    }

    void PPU::ppuWrite(uint16_t addr, uint8_t data)
    {
        addr &= 0x3FFF;
        if (addr >= 0x3F00 && addr <= 0x3FFF)
        {
            addr &= 0x001F;
            // Espelhamento de paleta: 0x3F10, 0x3F14... são espelhos de 0x3F00, 0x3F04...
            if (addr == 0x0010)
                addr = 0x0000;
            if (addr == 0x0014)
                addr = 0x0004;
            if (addr == 0x0018)
                addr = 0x0008;
            if (addr == 0x001C)
                addr = 0x000C;
            paletteTable[addr] = data;
        }
    }

    void PPU::clock() { /* Lógica de scanline */ }
    void PPU::reset()
    {
        addressLatch = 0;
        ppuAddress = 0;
    }
}