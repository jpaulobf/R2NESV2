#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "Common/Common.h"
#include "Core/Cartridge/Mappers/Mapper.h"
#include "Core/Cartridge/ROM/PRGROM.h"
#include "Core/Cartridge/ROM/CHRROM.h"

namespace R2NES::Core
{
    class Cartridge
    {
    public:
        Cartridge(const std::string &fileName);
        virtual ~Cartridge() = default;

        // Leitura/Escrita do lado da CPU
        bool cpuRead(uint16_t addr, uint8_t &data) const;
        bool cpuWrite(uint16_t addr, uint8_t data);

        // Leitura/Escrita do lado da PPU (Gráficos)
        bool ppuRead(uint16_t addr, uint8_t &data) const;
        bool ppuWrite(uint16_t addr, uint8_t data);

        // Retorna se o cartucho foi carregado com sucesso
        bool isValid() const { return imageValid; }

        MirrorMode getMirrorMode() const { return mirror; }

    private:
        std::unique_ptr<PRGROM> prgROM;
        std::unique_ptr<CHRROM> chrROM;
        std::shared_ptr<Mapper> pMapper;

        MirrorMode mirror = MirrorMode::HORIZONTAL;
        uint8_t mapperID = 0;
        uint8_t prgBanks = 0;
        uint8_t chrBanks = 0;

        bool imageValid = false;
    };
}