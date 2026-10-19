#include "CheatManager.h"
#include "Core/Bus/Bus.h"
#include "Core/Memory/RAM/RAM.h"
#include "Core/Cartridge/Cartridge.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>

namespace R2NES::Core
{
    namespace
    {
        std::string trim(const std::string &text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");
            return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        }

        std::string decodeDescription(std::string text)
        {
            // Decode ampersands last so nested entities are not decoded twice.
            for (const auto &entity : {std::pair{"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}})
            {
                size_t pos = 0;
                while ((pos = text.find(entity.first, pos)) != std::string::npos)
                {
                    text.replace(pos, std::char_traits<char>::length(entity.first), entity.second);
                    pos += std::char_traits<char>::length(entity.second);
                }
            }
            return text;
        }
    }

    bool CheatManager::decode(const std::string &text, Patch &patch)
    {
        std::string code = trim(text);
        std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c)
                       { return std::toupper(c); });
        patch = {};
        if (code.size() == 7 && code[4] == ':')
        {
            for (size_t i = 0; i < code.size(); ++i)
                if (i != 4 && !std::isxdigit(static_cast<unsigned char>(code[i])))
                    return false;
            patch.address = static_cast<uint16_t>(std::stoul(code.substr(0, 4), nullptr, 16));
            patch.value = static_cast<uint8_t>(std::stoul(code.substr(5), nullptr, 16));
            patch.ram = true;
            // Never freeze I/O registers or write into mapper control registers.
            return patch.address < 0x2000 || (patch.address >= 0x6000 && patch.address < 0x8000);
        }
        if (code.size() != 6 && code.size() != 8)
            return false;
        const std::string alphabet = "APZLGITYEOXUKSVN";
        uint8_t n[8] = {};
        for (size_t i = 0; i < code.size(); ++i)
        {
            const auto digit = alphabet.find(code[i]);
            if (digit == std::string::npos)
                return false;
            n[i] = static_cast<uint8_t>(digit);
        }
        patch.address = 0x8000 | ((n[3] & 7) << 12) | ((n[5] & 7) << 8) |
                        ((n[4] & 8) << 8) | ((n[2] & 7) << 4) | ((n[1] & 8) << 4) |
                        (n[4] & 7) | (n[3] & 8);
        patch.value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) |
                      ((code.size() == 8 ? n[7] : n[5]) & 8);
        if (code.size() == 8)
            patch.compare = ((n[7] & 7) << 4) | ((n[6] & 8) << 4) | (n[6] & 7) | (n[5] & 8);
        return true;
    }

    void CheatManager::clear()
    {
        enabled = false;
        entries.clear();
        romPatches.clear();
        ramPatches.clear();
    }

    void CheatManager::load(const std::filesystem::path &path)
    {
        clear();
        std::ifstream input(path);
        if (!input)
            return;
        std::map<unsigned long, Entry> parsed;
        const std::regex property(R"(^\s*cheat([0-9]+)_(desc|code|enable)\s*=\s*(.*?)\s*$)");
        std::string line;
        while (std::getline(input, line))
        {
            std::smatch match;
            if (!std::regex_match(line, match, property))
                continue;
            unsigned long index;
            try
            {
                index = std::stoul(match[1].str());
            }
            catch (const std::exception &)
            {
                continue;
            }
            auto value = trim(match[3].str());
            const auto field = match[2].str();
            if (field == "enable")
                parsed[index].enabled = value == "true";
            else if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            {
                value = value.substr(1, value.size() - 2);
                if (field == "desc")
                    parsed[index].description = decodeDescription(value);
                else
                    parsed[index].code = value;
            }
        }
        for (auto &[index, entry] : parsed)
        {
            if (entry.description.empty())
                entry.description = "Cheat " + std::to_string(index);
            std::istringstream codes(entry.code);
            std::string code;
            bool valid = !entry.code.empty() && entry.code.back() != '+';
            while (std::getline(codes, code, '+'))
            {
                Patch patch;
                if (!decode(code, patch))
                {
                    valid = false;
                    break;
                }
                entry.patches.push_back(patch);
            }
            if (!valid)
            {
                entry.patches.clear();
                entry.enabled = false;
                if (!entry.code.empty())
                    std::cerr << "Cheats: invalid code in " << path << ": " << entry.code << '\n';
            }
            entries.push_back(std::move(entry));
        }
        rebuild();
        std::cout << "Cheats: loaded " << entries.size() << " entries from " << path << '\n';
    }

    void CheatManager::rebuild()
    {
        romPatches.clear();
        ramPatches.clear();
        for (const auto &entry : entries)
            if (entry.enabled)
                for (const auto &patch : entry.patches)
                    if (patch.ram)
                        ramPatches.push_back(patch);
                    else
                        romPatches[patch.address].push_back(patch);
    }

    void CheatManager::setEntryEnabled(size_t index, bool value)
    {
        if (index >= entries.size() || !entries[index].valid())
            return;
        entries[index].enabled = value;
        rebuild();
    }

    uint8_t CheatManager::filterRead(uint16_t address, uint8_t original) const
    {
        if (!enabled || romPatches.empty())
            return original;
        const auto found = romPatches.find(address);
        if (found != romPatches.end())
            for (const auto &patch : found->second)
                if (patch.compare < 0 || patch.compare == original)
                    return patch.value;
        return original;
    }

    void CheatManager::applyRam(Bus &bus) const
    {
        if (enabled)
            for (const auto &patch : ramPatches)
            {
                if (patch.address < 0x2000 && bus.ram)
                    bus.ram->write(patch.address & 0x07FF, patch.value);
                else if (bus.cart)
                    bus.cart->cpuWrite(patch.address, patch.value, bus.systemClockCounter, bus.cpuCycleCounter);
            }
    }
}
