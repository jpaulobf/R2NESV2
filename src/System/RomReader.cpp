#include "System/RomReader.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>
namespace R2NES::System
{
    bool readRomFile(const std::string &path, size_t limit, std::vector<uint8_t> &data)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in)
            return false;
        const auto end = in.tellg();
        if (end < 0 || static_cast<uint64_t>(end) > limit)
            return false;
        std::vector<uint8_t> bytes(static_cast<size_t>(end));
        in.seekg(0);
        if (!in || (!bytes.empty() && !in.read(reinterpret_cast<char *>(bytes.data()), bytes.size())))
            return false;
        data = std::move(bytes);
        return true;
    }

    bool readRomImage(const std::string &path, std::vector<uint8_t> &data)
    {
        auto ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        if (ext == ".zip")
        {
            std::vector<uint8_t> archive;
            return readRomFile(path, Core::MaxZipFileSize, archive) && Core::extractRomFromZip(archive, data);
        }
        return (ext == ".nes" || ext == ".unf" || ext == ".unif") &&
               readRomFile(path, Core::MaxRomFileSize, data);
    }
}