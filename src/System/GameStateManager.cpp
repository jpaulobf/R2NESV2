#include "GameStateManager.h"
#include "Core/NES.h"
#include "Window/Window.h"
#include <filesystem>
#include <iostream>

namespace R2NES::System
{
    // Prepara o NES e a Window para uma nova ROM, incluindo o cache inicial do disassembly.
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
        if (nes.isCartridgeLoaded())
            cachedDisassembly = nes.getCpu().disassemble(0x8000, 0xFFFF);

        window.clearSelectedPath();
        window.setRomFile(std::filesystem::path(path).filename().string());
        window.setCartLoaded(true);
    }

    // Descarta o cartucho e remove da Window os estados associados à ROM anterior.
    void GameStateManager::unloadRom(Core::NES &nes, Core::Window &window)
    {
        std::cout << "GameStateManager: Unloading ROM..." << std::endl;
        nes.unload();
        cachedDisassembly.clear();
        window.clearUnloadRequest();
        window.uncheckZapperMenu();
        window.setRomFile("");
        window.setCartLoaded(false);
    }

    // Reinicia o hardware emulado após uma solicitação do menu.
    void GameStateManager::reset(Core::NES &nes, Core::Window &window)
    {
        std::cout << "GameStateManager: Resetting NES..." << std::endl;
        nes.reset();
        window.clearResetRequest();
    }

    // Persiste ou restaura o estado no slot solicitado e limpa as flags de comando da Window.
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

    // Reconstrói o cache somente quando o PC não está coberto pelo disassembly atual.
    void GameStateManager::updateDisassemblyCache(Core::NES &nes, uint16_t currentPC)
    {
        if (cachedDisassembly.find(currentPC) == cachedDisassembly.end())
        {
            cachedDisassembly = nes.getCpu().disassemble(0x8000, 0xFFFF);
        }
    }
}
