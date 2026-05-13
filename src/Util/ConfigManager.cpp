#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace R2NES::Core::Util
{
    std::map<std::string, std::string> ConfigManager::configValues;

    ConfigManager::ConfigManager()
    {
        loadConfigFile();
    }

    ConfigManager::~ConfigManager()
    {
        saveConfigFile();
    }

    void ConfigManager::loadConfigFile()
    {
        std::ifstream file(configFilePath);
        if (!file.is_open())
        {
            std::cout << "ConfigManager: Arquivo de config nao encontrado em: " << std::filesystem::absolute(configFilePath) << " (Sera criado ao fechar)" << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            // Ignora linhas vazias ou comentários
            if (line.empty() || line[0] == '#')
                continue;

            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos)
            {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                configValues[key] = value;
            }
        }

        // Popula a lista interna baseada nos campos f1-f10 salvos
        listOfRoms.clear();
        for (int i = 1; i <= 10; ++i)
        {
            std::string key = "f" + std::to_string(i);
            if (configValues.count(key) && !configValues[key].empty())
            {
                listOfRoms.push_back(configValues[key]);
            }
        }

        // Popula lastRomPath
        if (configValues.count("last_rom_path"))
        {
            lastRomPath = configValues["last_rom_path"];
        }
        else
        {
            lastRomPath = ""; // Ensure it's empty if not found
        }
    }

    void ConfigManager::saveConfigFile()
    {
        // Garante que a pasta 'resources' exista antes de tentar salvar
        std::filesystem::path path(configFilePath);
        if (path.has_parent_path() && !std::filesystem::exists(path.parent_path()))
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::cout << "ConfigManager: Salvando configuracoes em: " << std::filesystem::absolute(configFilePath) << std::endl;

        // Atualiza os valores f1-f10 no map antes de salvar
        int i = 1;
        for (const auto &rom : listOfRoms)
        {
            configValues["f" + std::to_string(i)] = rom;
            i++;
        }
        // Limpa slots remanescentes se a lista tiver menos de 10
        for (; i <= 10; ++i)
        {
            configValues["f" + std::to_string(i)] = "";
        }

        // Ensure last_rom_path is updated in the map before saving
        configValues["last_rom_path"] = lastRomPath;

        std::ofstream file(configFilePath);
        if (!file.is_open())
            return;

        file << "# Last Opened Files\n";
        for (int j = 1; j <= 10; ++j)
        {
            std::string key = "f" + std::to_string(j);
            file << key << "=" << configValues[key] << "\n";
        }

        // Salva outras configurações futuras que estiverem no map
        for (auto const &[key, val] : configValues)
        {
            if (key.size() > 0 && key[0] != 'f') // Evita duplicar f1-f10
            {
                file << key << "=" << val << "\n";
            }
        }
    }

    void ConfigManager::addRomToList(const std::string &romPath)
    {
        if (romPath.empty())
            return;

        // Se a ROM já estiver na lista, removemos para reinseri-la no topo (f1)
        listOfRoms.remove(romPath);

        // Adiciona no início (f1)
        listOfRoms.push_front(romPath);

        // Mantém apenas os 10 últimos
        if (listOfRoms.size() > 10)
        {
            listOfRoms.pop_back();
        }
    }
}