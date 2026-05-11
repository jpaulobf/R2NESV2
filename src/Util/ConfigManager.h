#pragma once

#include <string>
#include <map>
#include <list>

namespace R2NES::Core::Util
{
    class ConfigManager
    {
    public:
        ConfigManager();
        ~ConfigManager();

        void loadConfigFile();
        void saveConfigFile();
        void addRomToList(const std::string& romPath);
        const std::list<std::string>& getRecentRoms() const { return listOfRoms; }

    private:
        const std::string configFilePath = "resources/config.ini";
        std::list<std::string> listOfRoms;
        static std::map<std::string, std::string> configValues;
    };
}