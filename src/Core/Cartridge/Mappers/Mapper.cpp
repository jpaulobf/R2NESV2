#include "Core/Cartridge/Mappers/Mapper.h"

namespace R2NES::Core
{
    std::vector<uint8_t> Mapper::getBatteryData() const { return {}; }
    bool Mapper::setBatteryData(const std::vector<uint8_t> &data) { return data.empty(); }
    bool Mapper::convertBatteryData(const std::vector<uint8_t> &, std::vector<uint8_t> &, std::string &) const { return false; }
}
