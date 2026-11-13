#include "KeyboardConfig.h"
#include "Util/ConfigManager.h"
#include <array>
#include <charconv>
#include <string>

namespace R2NES::Core
{
    namespace
    {
        std::string key(int pad, int button) { return "keyboard_" + std::to_string(pad + 1) + "_" + std::to_string(button); }
    }
    R2Keyboard::Mapping KeyboardConfig::load() const
    {
        // Valores ausentes ou inválidos conservam o padrão de cada posição.
        auto mapping = R2Keyboard::defaults();
        for (int p = 0; p < 2; ++p)
            for (int b = 0; b < 10; ++b)
            {
                const auto value = config_.getValue(key(p, b));
                unsigned parsed = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (!value.empty() && result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
                    (parsed == 0 || R2Keyboard::assignable(parsed)))
                    mapping[p][b] = parsed;
            }
        return mapping;
    }
    bool KeyboardConfig::save(const R2Keyboard::Mapping &mapping)
    {
        std::array<std::array<std::string, 10>, 2> previous;
        for (int p = 0; p < 2; ++p)
            for (int b = 0; b < 10; ++b)
            {
                previous[p][b] = config_.getValue(key(p, b));
                config_.setValue(key(p, b), std::to_string(mapping[p][b]));
            }
        // Restaura os valores em memória se a gravação não for concluída.
        if (config_.saveConfigFile())
            return true;
        for (int p = 0; p < 2; ++p)
            for (int b = 0; b < 10; ++b)
                config_.setValue(key(p, b), previous[p][b]);
        return false;
    }
}