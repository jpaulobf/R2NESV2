#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace R2NES::Core
{
    class Cartridge;
}
namespace R2NES::System
{
    // Guarda a persistência de um cartucho sem assumir a posse do hardware.
    class BatterySession
    {
    public:
        BatterySession(std::shared_ptr<Core::Cartridge> cartridge, const std::filesystem::path &directory);
        ~BatterySession();
        bool flush();
        bool importBattery();
        bool canImport() const { return importAvailable_; }
        bool hasLoadError() const { return loadFailed_; }
        const std::string &message() const { return message_; }

    private:
        std::shared_ptr<Core::Cartridge> cartridge_;
        std::filesystem::path path_;
        std::vector<uint8_t> lastData_;
        bool loadFailed_ = false, importAvailable_ = false;
        std::string message_;
    };
}