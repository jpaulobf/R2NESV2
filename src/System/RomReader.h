#pragma once
#include "Core/Cartridge/RomFile.h"
namespace R2NES::System
{
    bool readRomFile(const std::string &path, size_t limit, std::vector<uint8_t> &data);
    bool readRomImage(const std::string &path, std::vector<uint8_t> &data);
}