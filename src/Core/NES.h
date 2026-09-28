#pragma once
#include "Core/Bus/Bus.h"
#include "Core/CPU/CPU.h"
#include "Core/PPU/PPU.h"
#include "Core/APU/APU.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include <string>
#include <iosfwd>
#include "Core/IO/Joysticks.h"

namespace R2NES::Core
{
    class NES
    {
    public:
        // Construtor / destrutor: inicializam os componentes do console e suas
        // conexões (bus, CPU, PPU, APU, RAM, Joysticks).
        NES();
        ~NES();

        // Executa um passo de emulação (avança clocks, processa DMA/NMI/IRQ e APU).
        void step();

        // Carrega um cartucho a partir de arquivo ROM (path). Valida o cartucho
        // antes de conectá-lo ao barramento.
        void insertCartridge(const std::string &path);

        // Reseta todo o estado do console para um estado limpo (cold boot simulation).
        void reset();

        // Remove o cartucho atualmente carregado e restaura estado básico.
        void unload();

        // Acesso aos subsistemas para depuração/inspeção.
        CPU &getCpu() { return cpu; }
        PPU &getPpu() { return ppu; }
        Bus &getBus() { return bus; }
        APU &getApu() { return apu; }

        // Leitura direta no barramento (útil para debug; não altera o estado)
        uint8_t cpuRead(uint16_t addr) { return bus.cpuRead(addr); }

        // Retorna o contador de ciclos do sistema (clock global)
        uint32_t getSystemClockCounter() const { return bus.systemClockCounter; }

        // Frame completo (PPU) e controle associado
        bool isFrameComplete() const { return ppu.isFrameComplete(); }
        void clearFrameComplete() { ppu.clearFrameComplete(); }

        bool isCartridgeLoaded() const { return cartridgeLoaded; }

        IO::Joysticks &getJoysticks() { return joysticks; }

        // Persistência de estado (savestate)
        bool saveState(const std::string &filename);
        bool loadState(const std::string &filename);
        bool saveState(std::ostream &output);
        bool loadState(std::istream &input);

        // Habilita/Desabilita renderização de tiles/sprites no PPU (útil para depuração)
        void setTilesEnabled(bool enabled);
        void setSpritesEnabled(bool enabled);

        // Permite ativar um modo experimental de overclock da CPU (para testes)
        void setCPUOverclock(bool enabled) { cpuOverclock = enabled; }

    private:
        // Subsistemas do console
        Bus bus;                 // Barramento do sistema, conecta todos os dispositivos
        RAM ram;                 // RAM de trabalho da CPU (2KB)
        CPU cpu;                 // Emulador da CPU 6502
        PPU ppu;                 // Emulador do PPU (video)
        APU apu;                 // Emulador do APU (audio)
        IO::Joysticks joysticks; // Entrada de controles

        // Estado interno
        bool cartridgeLoaded = false; // Indica se um cartucho está presente
        uint8_t nmi_delay = 0;        // Delay para disparo de NMI (proteção contra NMI hijacking)
        bool cpuOverclock = false;    // Flag de overclock experimental da CPU
    };
}