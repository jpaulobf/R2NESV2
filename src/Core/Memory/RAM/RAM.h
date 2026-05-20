#pragma once
#include <cstdint>
#include <vector>

namespace R2NES::Core
{
    class RAM
    {
    public:
        RAM(size_t size = 2048);
        ~RAM();

        void write(uint16_t addr, uint8_t data);

        uint8_t read(uint16_t addr) const;

        size_t getSize() const { return data.size(); }

    private:
        std::vector<uint8_t> data;
    };
}