#pragma once
#include <cstdint>
#include <vector>
#include <iostream>

namespace R2NES::Core
{
    class RAM
    {
    public:
        RAM(size_t size = 2048);
        ~RAM();

        // Escrita de um byte na RAM
        void write(uint16_t addr, uint8_t data);

        // Leitura de um byte da RAM
        uint8_t read(uint16_t addr) const;

        // Retorna o tamanho da RAM
        size_t getSize() const { return data.size(); }

        // Serialização para Save / Load states
        void saveState(std::ostream &os);
        void loadState(std::istream &is);
        
        // Limpa toda a memória RAM
        void reset();

    private:
        std::vector<uint8_t> data;
    };
}