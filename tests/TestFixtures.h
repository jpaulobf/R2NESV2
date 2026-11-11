#pragma once
#include "Core/NES.h"
#include "System/RomReader.h"
#include "System/BatterySession.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <memory>

namespace R2NES::Core
{
    inline std::vector<uint8_t> fixtureRom(const std::filesystem::path &path)
    {
        std::vector<uint8_t> data;
        System::readRomImage(path.string(), data);
        return data;
    }

    class FixtureCartridge : public Cartridge
    {
    public:
        explicit FixtureCartridge(const std::filesystem::path &path) : Cartridge(fixtureRom(path)) {}
        explicit FixtureCartridge(const std::vector<uint8_t> &bytes) : Cartridge(bytes) {}
        bool loadBattery(const std::filesystem::path &directory)
        {
            if (!battery_)
                battery_ = std::make_unique<System::BatterySession>(
                    std::shared_ptr<Cartridge>(this, [](Cartridge *) {}), directory);
            return !battery_->hasLoadError();
        }
        bool flushBattery() { return !battery_ || battery_->flush(); }
        bool canImportBattery() const { return battery_ && battery_->canImport(); }
        bool hasBatteryLoadError() const { return battery_ && battery_->hasLoadError(); }
        bool importBattery() { return battery_ && battery_->importBattery(); }
        const std::string &getBatteryMessage() const
        {
            static const std::string empty;
            return battery_ ? battery_->message() : empty;
        }

    private:
        std::unique_ptr<System::BatterySession> battery_;
    };

    class FixtureNES : public NES
    {
    public:
        explicit FixtureNES(std::filesystem::path directory = "battery") : batteryDirectory_(std::move(directory)) {}
        using NES::insertCartridge;
        bool insertCartridge(const std::string &path)
        {
            auto cartridge = std::make_shared<Cartridge>(fixtureRom(path));
            if (!cartridge->isValid())
                return false;
            flushBattery();
            auto next = std::make_unique<System::BatterySession>(cartridge, batteryDirectory_);
            if (!NES::insertCartridge(cartridge))
                return false;
            battery_ = std::move(next);
            const auto cheatPath = cheatDirectory_ / (cartridge->getRomHash() + ".cht");
            std::ifstream input(cheatPath);
            getBus().cheats.load(input, cheatPath.string());
            return true;
        }
        void setCheatDirectory(std::filesystem::path path) { cheatDirectory_ = std::move(path); }
        bool flushBattery() { return !battery_ || battery_->flush(); }
        void unload()
        {
            flushBattery();
            NES::unload();
            battery_.reset();
        }

    private:
        std::filesystem::path batteryDirectory_, cheatDirectory_ = "cheats";
        std::unique_ptr<System::BatterySession> battery_;
    };
}