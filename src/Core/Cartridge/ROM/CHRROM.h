#pragma once
#include <istream>
#include <ostream>
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

        void saveState(std::ostream &os) const
        {
            if (!memory.empty())
                os.write(reinterpret_cast<const char *>(memory.data()), static_cast<std::streamsize>(memory.size()));
        }

        void loadState(std::istream &is)
        {
            if (!memory.empty())
                is.read(reinterpret_cast<char *>(memory.data()), static_cast<std::streamsize>(memory.size()));
        }

        size_t size() const { return memory.size(); }

    private:
        std::vector<uint8_t> memory;
    };
}