#pragma once
#include <vector>
#include <cstdint>

namespace R2NES::Core
{
    class CHRROM
    {
    public:
        CHRROM(std::vector<uint8_t> data) : memory(std::move(data)) {}

        uint8_t read(uint32_t addr) const
        {
            return (addr < memory.size()) ? memory[addr] : 0x00;
        }

        void write(uint32_t addr, uint8_t data)
        {
            if (addr < memory.size())
                memory[addr] = data;
        }

        size_t size() const { return memory.size(); }

    private:
        std::vector<uint8_t> memory;
    };
}