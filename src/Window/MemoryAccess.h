#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace R2NES::Core
{
    // Acesso a bytes para os viewers, independente das classes da emulação.
    // Endereços são offsets de 16 bits; size deve ser no máximo 65536.
    // Os callbacks são usados apenas durante render(), sem serem armazenados.
    struct MemoryAccess
    {
        std::size_t size = 0;
        std::function<uint8_t(uint16_t)> read;
        // Opcional: a ausência de write torna a visualização somente leitura.
        std::function<void(uint16_t, uint8_t)> write;
    };
}