#include "GameStateManager.h"
#include "Core/NES.h"
#include "Window/Window.h"
#include <filesystem>
#include <iostream>

namespace R2NES::System
{
    void GameStateManager::loadRom(const std::string &path, Core::NES &nes, Core::Window &window)
    {
        window.uncheckZapperMenu();
        nes.unload();
        cachedDisassembly.clear();

        currentRomPath = path;
        std::cout << "GameStateManager: Loading ROM -> " << path << std::endl;
        nes.insertCartridge(path);
        nes.reset();

        window.setPaused(false);

        // Gera o disassembly apenas uma vez no carregamento
        cachedDisassembly = nes.getCpu().disassemble(0x8000, 0xFFFF);

        window.clearSelectedPath();
        window.setCartLoaded(true);
    }

    void GameStateManager::unloadRom(Core::NES &nes, Core::Window &window)
    {
        std::cout << "GameStateManager: Unloading ROM..." << std::endl;
        nes.unload();
        cachedDisassembly.clear();
        window.clearUnloadRequest();
        window.uncheckZapperMenu();
        window.setCartLoaded(false);
    }

    void GameStateManager::reset(Core::NES &nes, Core::Window &window)
    {
        std::cout << "GameStateManager: Resetting NES..." << std::endl;
        nes.reset();
        window.clearResetRequest();
    }

    void GameStateManager::handleSaveLoadState(Core::NES &nes, Core::Window &window)
    {
        if (nes.isCartridgeLoaded() && (window.getIsToSave() || window.getIsToLoad()))
        {
            namespace fs = std::filesystem;

            // 1. Prepara o nome do arquivo: [rom].[slot].sav
            std::string romName = fs::path(currentRomPath).stem().string();
            std::string slot = std::to_string(window.getSaveSlot());

            fs::create_directories("savestates"); // Garante que a pasta existe
            std::string filename = "savestates/" + romName + "." + slot + ".sav";

            if (window.getIsToSave())
            {
                if (nes.saveState(filename))
                    std::cout << "GameStateManager: State saved to " << filename << std::endl;
            }
            else if (window.getIsToLoad())
            {
                if (nes.loadState(filename))
                    std::cout << "GameStateManager: State loaded from " << filename << std::endl;
            }

            window.resetSaveLoadFlags();
        }
    }

    void GameStateManager::updateDisassemblyCache(Core::NES &nes, uint16_t currentPC)
    {
        if (cachedDisassembly.find(currentPC) == cachedDisassembly.end())
        {
            cachedDisassembly = nes.getCpu().disassemble(0x8000, 0xFFFF);
        }
    }
}
