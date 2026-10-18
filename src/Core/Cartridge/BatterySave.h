#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace R2NES::Core
{
    // Infraestrutura de arquivo independente do mapper e do formato de save state.
    class BatterySave
    {
    public:
        enum class LoadResult
        {
            Missing,
            Loaded,
            Error
        };
        static LoadResult load(const std::filesystem::path &path, std::vector<uint8_t> &data, std::string &error);
        static bool save(const std::filesystem::path &path, const std::vector<uint8_t> &data, std::string &error);
        static bool readFile(const std::filesystem::path &path, size_t maximumSize,
                             std::vector<uint8_t> &data, std::string &error);
        static bool backup(const std::filesystem::path &path, std::filesystem::path &destination, std::string &error);
    };
}
