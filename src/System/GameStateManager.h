#pragma once

#include <string>
#include <map>
#include <SDL.h>

namespace R2NES::Core
{
    class NES;
}
namespace R2NES::Core
{
    class Window;
}

namespace R2NES::System
{
    class GameStateManager
    {
    public:
        GameStateManager() = default;
        ~GameStateManager() = default;

        void loadRom(const std::string &path, Core::NES &nes, Core::Window &window);
        void unloadRom(Core::NES &nes, Core::Window &window);
        void reset(Core::NES &nes, Core::Window &window);

        void handleSaveLoadState(Core::NES &nes, Core::Window &window);

        const std::map<uint16_t, std::string> &getCachedDisassembly() const { return cachedDisassembly; }

        // Permite recarregar o disassembly caso o banco de memória mude
        void updateDisassemblyCache(Core::NES &nes, uint16_t currentPC);

    private:
        std::string currentRomPath;
        std::map<uint16_t, std::string> cachedDisassembly;
    };
}
