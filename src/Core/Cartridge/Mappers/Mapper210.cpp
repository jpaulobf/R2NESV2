#include "Core/Cartridge/Mappers/Mapper210.h"
#include <algorithm>
#include <cstring>

namespace R2NES::Core
{
    Mapper210::Mapper210(uint16_t p, uint16_t c, MirrorMode m, bool variant175, size_t ramSize, bool hasBattery)
        : Mapper(p, c), namco175(variant175), battery(hasBattery), initialMirror(m), mirror(m), ram(ramSize) { reset(); }

    void Mapper210::reset()
    {
        prg.fill(0);
        chr.fill(0);
        writeEnabled = false;
        mirror = initialMirror;
    }
    bool Mapper210::cpuMapRead(uint16_t a, uint32_t &mapped, uint8_t &data)
    {
        if (a >= 0x6000 && a < 0x8000 && !ram.empty())
        {
            data = ram[(a - 0x6000) % ram.size()];
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (a < 0x8000)
            return false;
        const unsigned slot = (a - 0x8000) / 0x2000;
        const unsigned count = unsigned(nPRGBanks) * 2;
        const unsigned bank = slot == 3 ? count - 1 : prg[slot] % count;
        mapped = bank * 0x2000 + (a & 0x1FFF);
        return true;
    }
    bool Mapper210::cpuMapWrite(uint16_t a, uint32_t &mapped, uint8_t value, uint32_t)
    {
        if (a >= 0x6000 && a < 0x8000 && !ram.empty())
        {
            if (writeEnabled)
                ram[(a - 0x6000) % ram.size()] = value;
            mapped = 0xFFFFFFFF;
            return true;
        }
        if (a < 0x8000)
            return false;
        if (a < 0xC000)
            chr[(a - 0x8000) >> 11] = value;
        else if ((a & 0xF800) == 0xC000 && namco175)
            writeEnabled = (value & 1) != 0;
        else if (a >= 0xE000 && a < 0xF800)
        {
            const unsigned slot = (a - 0xE000) >> 11;
            prg[slot] = value & 0x3F;
            if (slot == 0 && !namco175)
            {
                const MirrorMode modes[] = {MirrorMode::ONESCREEN_LO, MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_HI};
                mirror = modes[value >> 6];
            }
        }
        mapped = 0xFFFFFFFF;
        return true;
    }
    bool Mapper210::ppuMapRead(uint16_t a, uint32_t &mapped, uint8_t &, uint32_t)
    {
        if (a >= 0x2000)
            return false;
        mapped = (unsigned(chr[a >> 10]) % (unsigned(nCHRBanks) * 8)) * 0x400 + (a & 0x3FF);
        return true;
    }
    bool Mapper210::ppuMapWrite(uint16_t, uint32_t &, uint8_t, uint32_t) { return false; }
    void Mapper210::saveState(std::ostream &os)
    {
        os.write("N210", 4);
        os.write(reinterpret_cast<const char *>(prg.data()), prg.size());
        os.write(reinterpret_cast<const char *>(chr.data()), chr.size());
        os.put(char(writeEnabled));
        os.put(char(mirror));
        os.write(reinterpret_cast<const char *>(ram.data()), ram.size());
    }
    void Mapper210::loadState(std::istream &is)
    {
        auto next = *this;
        char tag[4];
        is.read(tag, 4);
        is.read(reinterpret_cast<char *>(next.prg.data()), next.prg.size());
        is.read(reinterpret_cast<char *>(next.chr.data()), next.chr.size());
        int enabled = is.get(), mode = is.get();
        is.read(reinterpret_cast<char *>(next.ram.data()), next.ram.size());
        bool validMode = mode == int(MirrorMode::HORIZONTAL) || mode == int(MirrorMode::VERTICAL) || mode == int(MirrorMode::ONESCREEN_LO) || mode == int(MirrorMode::ONESCREEN_HI);
        if (!is || std::memcmp(tag, "N210", 4) || enabled > 1 || enabled < 0 || !validMode ||
            (namco175 && mode != int(initialMirror)) || (!namco175 && enabled) ||
            std::any_of(next.prg.begin(), next.prg.end(), [](uint8_t b)
                        { return b > 63; }))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.writeEnabled = enabled != 0;
        next.mirror = MirrorMode(mode);
        *this = next;
    }
    std::vector<uint8_t> Mapper210::getBatteryData() const { return battery ? ram : std::vector<uint8_t>{}; }
    bool Mapper210::setBatteryData(const std::vector<uint8_t> &data)
    {
        if (!battery || data.size() != ram.size())
            return false;
        ram = data;
        return true;
    }
}