#include "Core/NESBoard.h"

namespace R2NES::Core {

    NesBoard::NesBoard() {
        // Conecta a RAM ao Bus
        bus.connectRam(&ram);
        // Conecta a CPU ao Bus
        cpu.connectBus(&bus);

        reset(); // Reseta o sistema para um estado inicial
    }

    void NesBoard::reset() {
        cpu.reset();
        systemClockCounter = 0;
        // ... (resetar PPU, APU, etc.)
    }

    void NesBoard::step() {
        // Aqui virá a lógica de sincronização
        // Por exemplo:
        // cpu.clock();
        // ppu.clock(); // PPU roda 3x mais rápido
        // apu.clock();
        // systemClockCounter++;
    }

    void NesBoard::insertCartridge(const std::string& path) {
        // Lógica para carregar a ROM e conectar ao Bus
    }
}