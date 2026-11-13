#pragma once
#include "System/KeyboardMapping.h"

namespace R2NES::Core::Util
{
    class ConfigManager;
}
namespace R2NES::Core
{
    // Lê e persiste as teclas, mantendo o formato do arquivo fora do diálogo.
    class KeyboardConfig
    {
    public:
        // O gerenciador deve permanecer válido durante a vida desta instância.
        explicit KeyboardConfig(Util::ConfigManager &config) : config_(config) {}
        // Usa os padrões nas posições ausentes ou com valores inválidos.
        R2Keyboard::Mapping load() const;
        // Retorna false se não for possível concluir a gravação do mapeamento.
        bool save(const R2Keyboard::Mapping &mapping);

    private:
        Util::ConfigManager &config_;
    };
}