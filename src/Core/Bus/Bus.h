#pragma once
#include <cstdint>

namespace R2NES::Core {
    class RAM;

    class Bus {
    public:
        Bus();
        ~Bus();

        // Conecta a RAM física ao barramento
        void connectRam(RAM* pRam);

        // Comunicação básica
        void cpuWrite(uint16_t addr, uint8_t data);
        uint8_t cpuRead(uint16_t addr, bool readOnly = false);

    public:
        RAM* ram = nullptr;
    };
}