#include "Window/KeyboardConfig.h"
#include "Util/ConfigManager.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
static void check(bool value)
{
    if (!value)
        throw std::runtime_error("keyboard config failed");
}
// Verifica padrões, persistência, valores inválidos e falhas de gravação.
int main()
{
    const auto original = fs::current_path();
    const auto dir = fs::temp_directory_path() / ("r2nes-keys-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    try
    {
        fs::current_path(dir);
        {
            R2NES::Core::Util::ConfigManager config;
            R2NES::Core::KeyboardConfig keys(config);
            auto mapping = keys.load();
            check(mapping == R2Keyboard::defaults());
            mapping[0][0] = SDLK_Q;
            mapping[1][4] = 0;
            config.setValue("unrelated", "preserved");
            check(keys.save(mapping));
        }
        {
            R2NES::Core::Util::ConfigManager config;
            R2NES::Core::KeyboardConfig keys(config);
            check(keys.load()[0][0] == SDLK_Q && keys.load()[1][4] == 0);
            check(config.getValue("unrelated") == "preserved");
            config.setValue("keyboard_1_0", "bad");
            check(keys.load()[0][0] == SDLK_W);
        }
        fs::create_directories(dir / "blocked");
        fs::current_path(dir / "blocked");
        {
            std::ofstream out("resources");
            out << "x";
        }
        {
            R2NES::Core::Util::ConfigManager config;
            R2NES::Core::KeyboardConfig keys(config);
            const auto before = keys.load();
            auto mapping = before;
            mapping[0][0] = SDLK_Q;
            check(!keys.save(mapping));
            check(keys.load() == before);
        }
        fs::current_path(original);
        fs::remove_all(dir);
    }
    catch (...)
    {
        fs::current_path(original);
        fs::remove_all(dir);
        throw;
    }
}