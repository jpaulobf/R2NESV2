#pragma once
#include <filesystem>
#include "Common/Common.h"
#include "Core/Cartridge/Mappers/Mapper.h"
#include "Core/Cartridge/ROM/CHRROM.h"
#include "Core/Cartridge/ROM/PRGROM.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <sstream>

namespace R2NES::Core
{
    class Cartridge
    {
    public:
        Cartridge(const std::string &fileName);
        virtual ~Cartridge();

        // A aplicação escolhe o diretório; construir um cartucho isolado não acessa saves.
        bool loadBattery(const std::filesystem::path &directory);
        bool flushBattery();
        bool canImportBattery() const { return batteryImportAvailable; }
        bool hasBatteryLoadError() const { return batteryLoadFailed; }
        bool importBattery();
        const std::string &getBatteryMessage() const { return batteryMessage; }

        // Leitura/Escrita do lado da CPU
        bool cpuRead(uint16_t addr, uint8_t &data, bool readOnly = false) const;
        void cpuReadOccurred(uint16_t addr, uint8_t data);
        bool cpuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter,
                      std::optional<uint32_t> cpuCycleCounter = std::nullopt);

        // Leitura/Escrita do lado da PPU (Gráficos)
        bool ppuRead(uint16_t addr, uint8_t &data, uint32_t systemClockCounter,
                     PpuReadContext context = PpuReadContext::Cpu) const;
        bool ppuPeek(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const;
        bool ppuReadSprite(uint16_t addr, uint8_t &data, uint32_t systemClockCounter) const;
        bool ppuWrite(uint16_t addr, uint8_t data, uint32_t systemClockCounter);
        void ppuAddressUpdated(uint16_t addr, uint32_t systemClockCounter);
        void ppuReadOccurred(uint16_t addr);
        void ppuScanlineStart();

        void saveState(std::ostream &os) const;
        void loadState(std::istream &is);

        // Retorna se o cartucho foi carregado com sucesso
        bool isValid() const { return imageValid; }

        // Retorna o hash CRC32 da ROM (excluindo o cabeçalho iNES)
        std::string getRomHash() const { return romHash; }
        uint16_t getMapperId() const { return mapperID; }

        // Chamados a cada ciclo pelo NES: mantém o encaminhamento visível
        // ao compilador, sem uma chamada intermediária fora de linha.
        void tick()
        {
            if (pMapper)
                pMapper->tick();
        }

        void clearIrqFlag();

        // Retorna se o Mapper está solicitando uma interrupção (IRQ)
        bool getIrqFlag() const { return pMapper && pMapper->getIrqFlag(); }

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
        uint16_t mapperID = 0;
        uint16_t prgBanks = 0;
        uint16_t chrBanks = 0;
        bool mapper2BusConflicts = true;
        bool mapper3BusConflicts = true;
        bool mapper7BusConflicts = false;
        std::string romHash;

        bool imageValid = false;
        std::filesystem::path batteryPath;
        std::vector<uint8_t> lastBatteryData;
        bool batteryLoadFailed = false;
        bool batteryImportAvailable = false;
        std::string batteryMessage;
    };
}
