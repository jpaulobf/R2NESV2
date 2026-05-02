#include "Core/PPU/PPU.h"
#include "Core/Cartridge/Cartridge.h"
#include <algorithm>

namespace R2NES::Core
{
    PPU::PPU()
    {
        std::fill(paletteTable.begin(), paletteTable.end(), 0x00);
        std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF000000); // Inicializa com preto opaco
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
        addr &= 0x0007; // Mapeia o intervalo $2000-$3FFF para os 8 registradores básicos
        // TODO: Implementar leitura de registradores como PPUSTATUS ($2002)
        return 0x00;
    }

    void PPU::cpuWrite(uint16_t addr, uint8_t data)
    {
        addr &= 0x0007; // Mapeia o intervalo $2000-$3FFF para os 8 registradores básicos
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
            // No NES, os endereços $3F10, $3F14, $3F18 e $3F1C são espelhos de 
            // $3F00, $3F04, $3F08 e $3F0C respectivamente (cores de fundo).
            if ((addr & 0x0013) == 0x0010) addr &= 0x000F;
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