#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace R2NES::Core
{
    // Normaliza as placas UNIF explicitamente suportadas, sem alterar a ROM original.
    bool decodeSachenUNIF(const std::vector<uint8_t> &input, std::vector<uint8_t> &ines, std::string &error);
}
