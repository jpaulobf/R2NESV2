#include "Core/Memory/RAM/RAM.h"
#include <algorithm>

namespace R2NES::Core {
    RAM::RAM(size_t size) {
        data.resize(size, 0x00);
    }

    RAM::~RAM() {
    }

    void RAM::write(uint16_t addr, uint8_t val) {
        if (addr < data.size()) {
            data[addr] = val;
        }
    }

    uint8_t RAM::read(uint16_t addr) const {
        return (addr < data.size()) ? data[addr] : 0x00;
    }
}