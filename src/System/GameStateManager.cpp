#include "GameStateManager.h"
#include "Core/NES.h"
#include "Core/Cartridge/RomFile.h"
#include "System/RomReader.h"
#include "Window/Window.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <zlib.h>

namespace
{
    void append32(std::vector<unsigned char> &bytes, uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
            bytes.push_back(static_cast<unsigned char>(value >> shift));
    }

    void appendChunk(std::vector<unsigned char> &png, const char *type,
                     const std::vector<unsigned char> &data)
    {
        append32(png, static_cast<uint32_t>(data.size()));
        const auto start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        append32(png, static_cast<uint32_t>(crc32(0, png.data() + start,
                                                  static_cast<uInt>(png.size() - start))));
    }

    bool saveThumbnail(const std::filesystem::path &path, const uint32_t *frame)
    {
        constexpr int border = 8;
        constexpr int frameWidth = 256, frameHeight = 240;
        constexpr int width = 128, height = 120;
        constexpr int croppedWidth = frameWidth - 2 * border;
        constexpr int croppedHeight = frameHeight - 2 * border;
        std::vector<unsigned char> raw;
        raw.reserve(height * (1 + width * 3));
        for (int y = 0; y < height; ++y)
        {
            raw.push_back(0); // PNG filter: none
            for (int x = 0; x < width; ++x)
            {
                const int sourceX = border + x * croppedWidth / width;
                const int sourceY = border + y * croppedHeight / height;
                const uint32_t pixel = frame[sourceY * frameWidth + sourceX];
                raw.push_back(static_cast<unsigned char>(pixel >> 16));
                raw.push_back(static_cast<unsigned char>(pixel >> 8));
                raw.push_back(static_cast<unsigned char>(pixel));
            }
        }
        uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
        std::vector<unsigned char> compressed(compressedSize);
        if (compress2(compressed.data(), &compressedSize, raw.data(),
                      static_cast<uLong>(raw.size()), Z_DEFAULT_COMPRESSION) != Z_OK)
            return false;
        compressed.resize(compressedSize);

        std::vector<unsigned char> png{137, 80, 78, 71, 13, 10, 26, 10};
        std::vector<unsigned char> header;
        append32(header, width);
        append32(header, height);
        header.insert(header.end(), {8, 2, 0, 0, 0});
        appendChunk(png, "IHDR", header);
        appendChunk(png, "IDAT", compressed);
        appendChunk(png, "IEND", {});
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
        output.close();
        return static_cast<bool>(output);
    }
}

namespace R2NES::System
{
    // Prepara o NES e a Window para uma nova ROM, incluindo o cache inicial do disassembly.
    bool GameStateManager::loadRom(const std::string &path, Core::NES &nes, Core::Window &window)
    {
        if (nes.isCartridgeLoaded() && !flushBattery(window))
        {
            // Uma falha de escrita não deve descartar a sessão com progresso novo.
            window.clearSelectedPath();
            return false;
        }
        std::cout << "GameStateManager: Loading ROM -> " << path << std::endl;
        // A aplicação lê e descompacta o arquivo; o cartucho recebe somente bytes.
        std::vector<uint8_t> bytes;
        const bool read = readRomImage(path, bytes);
        if (!read || !nes.insertCartridge(std::make_shared<Core::Cartridge>(bytes)))
        {
            window.clearSelectedPath();
            return false;
        }
        battery = std::make_unique<BatterySession>(nes.getBus().cart, batteryDirectory);
        const auto cheatPath = cheatDirectory / (nes.getBus().cart->getRomHash() + ".cht");
        std::ifstream cheats(cheatPath);
        nes.getBus().cheats.load(cheats, cheatPath.string());
        window.uncheckZapperMenu();
        nes.getJoysticks().port2Device = Core::IO::DeviceType::Gamepad;
        cachedDisassembly.clear();

        currentRomPath = path;
        if (nes.isCartridgeLoaded())
        {
            if (battery->canImport())
            {
                if (window.confirmBatteryImport(battery->message()))
                {
                    battery->importBattery();
                    window.showBatteryMessage(battery->message());
                }
            }
            else if (!battery->message().empty())
                window.showBatteryMessage(battery->message());
        }
        nes.reset();

        window.setPaused(false);

        // Gera o disassembly apenas uma vez no carregamento
        if (nes.isCartridgeLoaded())
            cachedDisassembly = nes.getCpu().disassemble(0x8000, 0xFFFF);

        window.clearSelectedPath();
        window.setRomFile(nes.isCartridgeLoaded() ? std::filesystem::path(path).filename().string() : "");
        window.setCartLoaded(nes.isCartridgeLoaded());
        return true;
    }

    bool GameStateManager::flushBattery(Core::Window &window)
    {
        if (!battery || battery->flush() || battery->hasLoadError())
            return true;
        window.showBatteryMessage(battery->message());
        return false;
    }

    // Descarta o cartucho e remove da Window os estados associados à ROM anterior.
    void GameStateManager::unloadRom(Core::NES &nes, Core::Window &window)
    {
        if (nes.isCartridgeLoaded() && !flushBattery(window))
        {
            window.clearUnloadRequest();
            return;
        }
        std::cout << "GameStateManager: Unloading ROM..." << std::endl;
        nes.unload();
        battery.reset();
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
    bool GameStateManager::handleSaveLoadState(Core::NES &nes, Core::Window &window)
    {
        bool stateLoaded = false;
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
                std::ofstream output(filename, std::ios::binary);
                if (output && nes.saveState(output) && output.flush())
                {
                    std::cout << "GameStateManager: State saved to " << filename << std::endl;
                    const char *basePath = SDL_GetBasePath();
                    const fs::path directory = (basePath ? fs::path(basePath) : fs::current_path()) / "savestate-thumbs";
                    const fs::path thumbnail = directory / (romName + "." + slot + ".png");
                    std::error_code error;
                    fs::create_directories(directory, error);
                    if (error || !saveThumbnail(thumbnail, nes.getPpu().getFrameBuffer()))
                    {
                        // Não deixar uma imagem antiga associada ao novo estado.
                        fs::remove(thumbnail, error);
                        std::cerr << "GameStateManager: Could not save thumbnail " << thumbnail << std::endl;
                    }
                }
            }
            else if (window.getIsToLoad())
            {
                std::ifstream input(filename, std::ios::binary);
                if (input && nes.loadState(input))
                {
                    std::cout << "GameStateManager: State loaded from " << filename << std::endl;
                    stateLoaded = true;
                }
            }

            window.resetSaveLoadFlags();
        }
        return stateLoaded;
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