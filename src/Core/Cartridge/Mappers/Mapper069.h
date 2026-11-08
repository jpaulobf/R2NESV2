#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <array>

namespace R2NES::Core
{
    // Controlador de memória Sunsoft FME-7; o áudio de expansão 5B não é emulado.
    class Mapper069 : public Mapper
    {
    public:
        // Recebe a quantidade de bancos de PRG/CHR, a presença de RAM e bateria
        // e o espelhamento inicial informado pelo cartucho.
        Mapper069(uint16_t prg, uint16_t chr, bool ram, bool battery, MirrorMode mirror);

        // Mapeia leituras da CPU para PRG-ROM ou atende diretamente pela RAM.
        bool cpuMapRead(uint16_t, uint32_t &, uint8_t &) override;

        // Escreve na RAM habilitada ou nos registradores de comando e dados.
        bool cpuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;

        // Mapeia leituras da PPU para um dos oito bancos de CHR de 1 KiB.
        bool ppuMapRead(uint16_t, uint32_t &, uint8_t &, uint32_t) override;

        // Permite escrita no CHR somente quando o cartucho usa CHR-RAM.
        bool ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) override;

        // Retorna o espelhamento de nametables selecionado pelo registrador.
        MirrorMode getMirrorMode() override;

        // Indica se o contador solicitou uma interrupção à CPU.
        bool getIrqFlag() const override { return irq; }

        // Avança o contador de IRQ em um ciclo, quando habilitado.
        void tick() override;
        float expansionAudio() const override;

        // Reinicia registradores e IRQ, preservando o conteúdo da RAM.
        void reset() override;

        // Salva registradores, espelhamento e RAM no estado do emulador.
        void saveState(std::ostream &) override;

        // Valida e restaura um estado; sinaliza falha no fluxo se for inválido.
        void loadState(std::istream &) override;

        // Retorna a RAM persistente quando o cartucho possui bateria.
        std::vector<uint8_t> getBatteryData() const override;

        // Restaura os dados de bateria se o tamanho corresponder ao esperado.
        bool setBatteryData(const std::vector<uint8_t> &) override;

    private:
        bool hasRam, battery; // Presença de RAM e de bateria no cartucho.
        MirrorMode initialMirror, mirror;
        std::array<uint8_t, 8192> ram{};
        std::array<uint8_t, 8> chr{};     // Seleção dos oito bancos CHR de 1 KiB.
        std::array<uint8_t, 4> prg{};     // Bancos PRG de 8 KiB; prg[0] inclui controle da RAM.
        uint8_t command = 0, control = 0; // Comando selecionado e controle do contador/IRQ.
        uint16_t counter = 0;
        bool irq = false;
        std::array<uint8_t, 16> audioRegs{};
        std::array<uint16_t, 3> toneCounter{};
        std::array<uint8_t, 3> toneOutput{};
        uint8_t audioSelect = 0, audioDivider = 0, noiseCounter = 0;
        uint16_t envelopeCounter = 0;
        uint32_t noiseLfsr = 1;
        uint8_t envelopeStep = 31, envelopeAttack = 0;
        bool envelopeHolding = false;
        void audioTick();
        void audioWrite(uint8_t value);
    };
}