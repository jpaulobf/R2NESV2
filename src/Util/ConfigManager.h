#pragma once

#include <string>
#include <map>
#include <list>

namespace R2NES::Core::Util
{
    // Persiste preferências simples e a lista de ROMs recentes em um arquivo INI.
    class ConfigManager
    {
    public:
        // Carrega as configurações disponíveis ao criar o gerenciador.
        ConfigManager();

        // Salva o estado atual antes de destruir o gerenciador.
        ~ConfigManager();

        // Lê pares chave=valor do arquivo de configuração e recompõe os valores derivados.
        void loadConfigFile();

        // Serializa as ROMs recentes e demais chaves conhecidas para o arquivo INI.
        void saveConfigFile();

        // Move a ROM informada para o topo da lista de recentes, limitada a dez entradas.
        void addRomToList(const std::string &romPath);

        // Retorna as ROMs em ordem de uso, da mais recente para a mais antiga.
        const std::list<std::string> &getRecentRoms() const { return listOfRoms; }

        // Retorna o último diretório utilizado pelo diálogo de abertura.
        const std::string &getLastRomPath() const { return lastRomPath; }

        // Atualiza o diretório persistido para a próxima abertura de ROM.
        void setLastRomPath(const std::string &path)
        {
            configValues["last_rom_path"] = path;
            lastRomPath = path;
        }

    private:
        // Caminho relativo do arquivo de configuração distribuído com a aplicação.
        const std::string configFilePath = "resources/config.ini";

        // Lista LRU de ROMs, refletida nas chaves f1 a f10 do arquivo.
        std::list<std::string> listOfRoms;

        // Diretório previamente escolhido pelo usuário no seletor de ROMs.
        std::string lastRomPath;

        // Armazena pares genéricos para preservar configurações além das ROMs recentes.
        static std::map<std::string, std::string> configValues;
    };
}