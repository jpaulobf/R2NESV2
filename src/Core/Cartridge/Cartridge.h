#pragma once
#include "Common/Common.h"
#include "Core/Cartridge/Mappers/Mapper.h"
#include "Core/Cartridge/ROM/CHRROM.h"
#include "Core/Cartridge/ROM/PRGROM.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

namespace R2NES::Core
{
    class Cartridge
    {
    public:
        Cartridge(const std::string &fileName);
        virtual ~Cartridge() = default;

        // Leitura/Escrita do lado da CPU
        bool cpuRead(uint16_t addr, uint8_t &data) const;
        bool cpuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter);

        // Leitura/Escrita do lado da PPU (Gráficos)
        bool ppuRead(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const;
        bool ppuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter);

        // Retorna se o cartucho foi carregado com sucesso
        bool isValid() const { return imageValid; }

        // Retorna o hash CRC32 da ROM (excluindo o cabeçalho iNES)
        std::string getRomHash() const { return romHash; }

        void tick();

        void clearIrqFlag();

        // Retorna se o Mapper está solicitando uma interrupção (IRQ)
        bool getIrqFlag() const;

        // Retorna o modo de espelhamento (Mirror Mode) definido pelo cartucho
        MirrorMode getMirrorMode() const;

        // Retorna o ponteiro para o Mapper do cartucho
        std::shared_ptr<Mapper> getMapper() { return pMapper; }

    private:
        std::string calculateRomHash(const std::vector<uint8_t> &buffer);
        bool loadFromBuffer(const std::vector<uint8_t> &buffer);
        std::unique_ptr<PRGROM> prgROM;
        std::unique_ptr<CHRROM> chrROM;
        std::shared_ptr<Mapper> pMapper;

        MirrorMode mirror = MirrorMode::HORIZONTAL;
        uint8_t mapperID = 0;
        uint8_t prgBanks = 0;
        uint8_t chrBanks = 0;
        std::string romHash;

        bool imageValid = false;
    };
}