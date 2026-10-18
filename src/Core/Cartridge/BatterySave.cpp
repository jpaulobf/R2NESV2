#include "Core/Cartridge/BatterySave.h"
#include <fstream>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace R2NES::Core
{
    bool BatterySave::readFile(const std::filesystem::path &path, size_t maximumSize,
                               std::vector<uint8_t> &data, std::string &error)
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || !size || size > maximumSize)
        {
            error = ec ? ec.message() : "Battery file is empty or exceeds the import limit";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        std::vector<uint8_t> next(static_cast<size_t>(size));
        input.read(reinterpret_cast<char *>(next.data()), next.size());
        if (!input || input.peek() != std::char_traits<char>::eof() || input.bad())
        {
            error = "Could not read complete battery file";
            return false;
        }
        data.swap(next);
        return true;
    }

    bool BatterySave::backup(const std::filesystem::path &path, std::filesystem::path &destination, std::string &error)
    {
        // copy_file sem overwrite preserva inclusive backups de importações anteriores.
        for (unsigned index = 0; index < 1000; ++index)
        {
            auto next = path;
            next += ".bak" + (index ? "." + std::to_string(index) : "");
            std::error_code ec;
            if (std::filesystem::copy_file(path, next, std::filesystem::copy_options::none, ec))
            {
                destination = next;
                return true;
            }
            if (ec != std::errc::file_exists)
            {
                error = ec.message();
                return false;
            }
        }
        error = "No unused battery backup filename available";
        return false;
    }

    BatterySave::LoadResult BatterySave::load(const std::filesystem::path &path,
                                              std::vector<uint8_t> &data, std::string &error)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec)
        {
            error = ec.message();
            return LoadResult::Error;
        }
        if (!exists)
            return LoadResult::Missing;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size != data.size())
        {
            error = ec ? ec.message() : "Battery file size does not match installed NVRAM";
            return LoadResult::Error;
        }
        std::ifstream input(path, std::ios::binary);
        std::vector<uint8_t> next(data.size());
        input.read(reinterpret_cast<char *>(next.data()), next.size());
        if (!input || input.peek() != std::char_traits<char>::eof() || input.bad())
        {
            error = "Could not read complete battery file";
            return LoadResult::Error;
        }
        data.swap(next); // Falha de leitura nunca aplica uma memória parcial.
        return LoadResult::Loaded;
    }

    bool BatterySave::save(const std::filesystem::path &path,
                           const std::vector<uint8_t> &data, std::string &error)
    {
        std::error_code ec;
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = ec.message();
            return false;
        }
        static std::atomic<uint64_t> sequence{0};
        auto temporary = path;
        temporary += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "." + std::to_string(sequence++) + ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char *>(data.data()), data.size());
            output.flush();
            output.close();
            if (!output)
            {
                error = "Could not write complete battery file";
                std::filesystem::remove(temporary, ec);
                return false;
            }
        }
        // Substitui o destino só após concluir a escrita. No Windows, rename
        // não substitui arquivos existentes; MoveFileEx preserva esse contrato.
#ifdef _WIN32
        const bool replaced = MoveFileExW(temporary.c_str(), path.c_str(),
                                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!replaced)
            error = std::error_code(GetLastError(), std::system_category()).message();
#else
        std::filesystem::rename(temporary, path, ec);
        const bool replaced = !ec;
        if (!replaced)
            error = ec.message();
#endif
        if (!replaced)
            std::filesystem::remove(temporary, ec);
        return replaced;
    }
}
