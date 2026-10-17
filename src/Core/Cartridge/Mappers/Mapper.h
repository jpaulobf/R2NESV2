#pragma once
#include <cstdint>
#include "Common/Common.h" // Ou onde seu MirrorMode estiver definido
#include <iostream>
#include <istream>

namespace R2NES::Core
{
    class VRAM;
    class Mapper
    {
    public:
        Mapper(uint16_t prgBanks, uint16_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
        virtual ~Mapper() = default;

        // Transforma o endereço virtual da CPU em um endereço físico na PRG-ROM
        virtual bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) = 0;
        virtual bool cpuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
        {
            return cpuMapRead(addr, mapped_addr, data);
        }
        // Observação do valor efetivamente devolvido por outro dispositivo (ex.: PPUSTATUS).
        virtual void onCpuRead(uint16_t addr, uint8_t data) {}
        virtual bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) = 0;

        // Transforma o endereço virtual da PPU em um endereço físico na CHR-ROM
        virtual bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) = 0;
        // Consulta do visualizador. Mappers com efeitos de leitura devem sobrescrever.
        virtual bool ppuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
        {
            return ppuMapRead(addr, mapped_addr, data, systemClockCounter);
        }
        virtual bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) = 0;

        virtual void setPpuReadContext(PpuReadContext context) {}
        // Compatibilidade com chamadas antigas que identificavam apenas sprites.
        virtual void setPpuReadIsSprite(bool isSprite)
        {
            setPpuReadContext(isSprite ? PpuReadContext::Sprite : PpuReadContext::Cpu);
        }

        // Notifica a mudança dos pinos de endereço da PPU independentemente
        // de o acesso ter sido atendido pelo mapeamento CHR do cartucho.
        virtual void onPpuAddress(uint16_t, uint32_t) {}
        // Pulso de leitura externa, separado da simples apresentação do endereço.
        virtual void onPpuRead(uint16_t addr) {}
        // Posição do tile que o pipeline está preparando, inclusive prefetch da próxima linha.
        virtual void onPpuBackgroundTile(int16_t scanline, uint8_t column) {}
        // Mappers que atendem todas as nametables podem expor sua CIRAM autoritativa.
        virtual VRAM *getCiram() { return nullptr; }

        // Notifica o início de uma scanline visível da PPU.
        virtual void onPpuScanlineStart() {}

        // Retorna se o Mapper está solicitando uma interrupção (IRQ)
        virtual bool getIrqFlag() const { return false; }

        // Hook de acknowledge do mapper; alguns chips exigem escrita em registrador.
        virtual void clearIrqFlag() {}

        // Realiza o tick
        virtual void tick() {}

        // Serialização para Save / Load states
        virtual void saveState(std::ostream &os) = 0;
        virtual void loadState(std::istream &is) = 0;

        // Define o estado inicial do mapper. Virtual puro não é necessário aqui,
        // permitindo que mappers simples não precisem implementá-lo.
        virtual void reset() {}

        // Por padrão, mappers retornam HARDWARE (indicando o que está no iNES)
        virtual MirrorMode getMirrorMode()
        {
            return MirrorMode::HORIZONTAL; // Fallback padrão
        }

    protected:
        uint16_t nPRGBanks = 0;
        uint16_t nCHRBanks = 0;
    };
}
