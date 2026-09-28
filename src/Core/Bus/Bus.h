#pragma once
#include <cstdint>
#include <memory>
#include "Common/Common.h"
#include "Core/IO/Joysticks.h"
#include "Core/CPU/CPU.h"

namespace R2NES::Core
{
    class RAM;
    class Cartridge;
    class PPU;
    class APU;
    class CPU;

    namespace IO
    {
        class Joysticks;
    }

    class Bus
    {
    public:
        // Construtor / Destrutor do barramento do sistema.
        // O `Bus` é o ponto central que conecta CPU, PPU, APU,
        // RAM, Cartucho e dispositivos de I/O.
        Bus();
        ~Bus();

        // Endereços dos registradores mapeados da PPU (Memory Mapped I/O).
        // Usados pela CPU para controlar o chip PPU.
        enum PPU_REGISTERS : uint16_t
        {
            PPUCTRL = 0x2000,
            PPUMASK = 0x2001,
            PPUSTATUS = 0x2002,
            OAMADDR = 0x2003,
            OAMDATA = 0x2004,
            PPUSCROLL = 0x2005,
            PPUADDR = 0x2006,
            PPUDATA = 0x2007
        };

        // Liga a CPU ao barramento para leituras/escritas.
        void connectCPU(CPU *pCpu);

        // Liga a RAM física ao barramento.
        void connectRam(RAM *pRam);

        // Liga a APU ao barramento (áudio).
        void connectAPU(APU *pApu);

        // Escrita da CPU via barramento (endereços CPU space).
        // Trata redirecionamentos para cartucho, APU, PPU e RAM.
        void cpuWrite(uint16_t addr, uint8_t data);

        // Leitura pela CPU via barramento. `readOnly` é usado
        // quando a operação não deve causar efeitos colaterais.
        uint8_t cpuRead(uint16_t addr, bool readOnly = false);

        // Leitura/escrita pela PPU (área de VRAM/cartucho).
        // Retornam true se o cartucho tratou a operação.
        bool ppuRead(uint16_t addr, uint8_t &data) const;
        bool ppuReadSprite(uint16_t addr, uint8_t &data) const;
        bool ppuWrite(uint16_t addr, uint8_t data);

        // Notificação de início de scanline para o cartucho (mappers que usam scanline IRQs).
        void ppuScanlineStart();

        // Espelhamento de nome de página (horizontal/vertical) fornecido pelo cartucho.
        MirrorMode getMirrorMode() const;

        // Inserir/Remover cartucho em tempo de execução.
        void setCartridge(const std::shared_ptr<Cartridge> &cartridge);

        // Conecta os dispositivos de entrada (joysticks / zapper).
        void connectJoysticks(IO::Joysticks *joysticks);

        // Conecta a PPU ao barramento (usada para leituras/escritas CPU->PPU).
        void connectPPU(PPU *pPpu);

        // Usado para informar o estado do gatilho da pistola Zapper.
        void setZapperTrigger(bool pulled);

        // Chamada quando o endereço da PPU foi atualizado (útil para debug/overlays).
        void ppuAddressUpdated(uint16_t addr);

    public:
        // Ponteiros para componentes conectados (podem ser null se não conectados).
        RAM *ram = nullptr;
        PPU *ppu = nullptr;
        APU *apu = nullptr;
        CPU *cpu = nullptr;
        IO::Joysticks *joysticks = nullptr;
        std::shared_ptr<Cartridge> cart;

        // Contador de ciclos do sistema (clock global do emulador).
        uint32_t systemClockCounter = 0;

        // Registros para a transferência DMA (OAM DMA 0x4014).
        // `dma_transfer` indica que uma DMA está em andamento.
        uint8_t dma_page = 0x00;
        uint8_t dma_addr = 0x00;
        uint8_t dma_data = 0x00;
        bool dma_transfer = false;
        bool dma_dummy = true;

    private:
        // Estado interno da Pistola Zapper (gatilho)
        bool zapperTrigger = false;
    };
}