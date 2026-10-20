#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace R2NES::Core
{
    class Bus;

    class CheatManager
    {
    public:
        struct Patch
        {
            uint16_t address = 0;
            uint8_t value = 0;
            int compare = -1;
            bool ram = false;
        };
        struct Entry
        {
            std::string description;
            std::string code;
            bool enabled = false;
            std::vector<Patch> patches;
            bool valid() const { return !patches.empty(); }
        };

        void load(const std::filesystem::path &path);
        void clear();
        bool isEnabled() const { return enabled; }
        // Permite pular a aplicação por ciclo quando não há RAM a congelar.
        bool hasActiveRamPatches() const { return enabled && !ramPatches.empty(); }
        void setEnabled(bool value) { enabled = value; }
        void setEntryEnabled(size_t index, bool value);
        const std::vector<Entry> &getEntries() const { return entries; }
        uint8_t filterRead(uint16_t address, uint8_t original) const;
        void applyRam(Bus &bus) const;

    private:
        static bool decode(const std::string &code, Patch &patch);
        void rebuild();
        bool enabled = false;
        std::vector<Entry> entries;
        std::unordered_map<uint16_t, std::vector<Patch>> romPatches;
        std::vector<Patch> ramPatches;
    };
}
