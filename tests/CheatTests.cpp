#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    try
    {
        check(argc == 3, "Expected fixture directory and sample cheat file");
        const std::filesystem::path directory = argv[1];
        std::filesystem::create_directories(directory);
        const auto romPath = directory / "cheat-test.nes";
        std::vector<uint8_t> rom(16 + 0x4000, 0);
        rom[0] = 'N';
        rom[1] = 'E';
        rom[2] = 'S';
        rom[3] = 0x1A;
        rom[4] = 1;
        rom[16 + 0x0CAE] = 0xA5; // SEXEVGSZ: $8CAE, replace $A5 with $85.
        rom[16 + 0x11D9] = 0xCE; // SXIOPO: $91D9, replace unconditionally with $AD.
        {
            std::ofstream output(romPath, std::ios::binary);
            output.write(reinterpret_cast<const char *>(rom.data()), rom.size());
        }
        Cartridge cartridge(romPath.string());
        check(cartridge.isValid(), "Invalid synthetic NROM");
        const auto cheatPath = directory / (cartridge.getRomHash() + ".cht");
        {
            std::ofstream output(cheatPath);
            output << "cheats = 99\n"
                   << "cheat0_desc = \"Health &quot;test&quot;\"\n"
                   << "cheat0_code = \"SEXEVGSZ\"\ncheat0_enable = true\n"
                   << "cheat2_desc = \"Lives\"\ncheat2_code = \"sx iopo\"\n"
                   << "cheat2_code = \" sxiopo \"\ncheat2_enable = false\n"
                   << "cheat4_desc = \"RAM pair\"\ncheat4_code = \"003C:40+0084:99\"\n"
                   << "cheat5_code = \"003C:40+BAD\"\ncheat5_enable = true\n"
                   << "cheat6_code = \"\"\n"
                   << "cheat7_code = \"2000:FF\"\n"
                   << "cheat8_code = \"003C:40+\"\n"
                   << "cheat9_code = \"SEXEVGSA\"\n" // Same address, different comparison byte.
                   << "cheat10_code = \"183C:22\"\n" // Internal RAM mirror.
                   << "cheat99999999999999999999999999999_code = \"SXIOPO\"\n";
        }
        NES nes(directory / "battery");
        nes.setCheatDirectory(directory);
        nes.insertCartridge(romPath.string());
        auto &bus = nes.getBus();
        auto &cheats = bus.cheats;
        check(nes.isCartridgeLoaded(), "ROM load failed");
        check(cheats.getEntries().size() == 9, "Sparse indices must not depend on declared count");
        check(cheats.getEntries()[0].description == "Health \"test\"", "Description entity decoding");
        check(!cheats.isEnabled(), "Master switch must start disabled");
        check(bus.cpuRead(0x8CAE) == 0xA5, "Disabled GG affected ROM");
        cheats.setEnabled(true);
        check(bus.cpuRead(0x8CAE) == 0x85 && bus.cpuRead(0x8CAE, true) == 0x85, "8-letter GG or debugger read failed");
        check(bus.cpuRead(0xCCAE) == 0xA5, "GG must not patch underlying mirrored ROM");
        cheats.setEntryEnabled(0, false);
        cheats.setEntryEnabled(7, true);
        check(bus.cpuRead(0x8CAE) == 0xA5, "8-letter GG ignored comparison byte");
        cheats.setEntryEnabled(1, true);
        check(bus.cpuRead(0x91D9) == 0xAD, "6-letter GG decoding failed");
        check(bus.cart->getRomHash() == cartridge.getRomHash(), "Cheats changed ROM hash");
        for (size_t i = 3; i <= 6; ++i)
        {
            check(!cheats.getEntries()[i].valid(), "Invalid compound, empty, or I/O code accepted");
            cheats.setEntryEnabled(i, true);
            check(!cheats.getEntries()[i].enabled, "Invalid code enabled");
        }
        cheats.setEntryEnabled(2, true);
        bus.cpuDataBus = 0xAB;
        cheats.applyRam(bus);
        check(bus.cpuDataBus == 0xAB, "RAM cheat polluted CPU open bus");
        check(bus.ram->read(0x3C) == 0x40 && bus.ram->read(0x84) == 0x99, "Compound RAM cheat failed");
        bus.cpuWrite(0x3C, 0);
        nes.step();
        check(bus.ram->read(0x3C) == 0x40, "RAM freeze not enforced while emulating");
        cheats.setEnabled(false);
        bus.cpuWrite(0x3C, 1);
        nes.step();
        check(bus.ram->read(0x3C) == 1 && bus.cpuRead(0x91D9) == 0xCE, "Master off did not suspend cheats");
        cheats.setEntryEnabled(2, false);
        cheats.setEntryEnabled(8, true);
        cheats.setEnabled(true);
        cheats.applyRam(bus);
        check(bus.ram->read(0x3C) == 0x22, "RAM mirror address failed");
        nes.reset();
        check(cheats.isEnabled(), "Reset unexpectedly disabled cheats");
        nes.step();
        check(bus.ram->read(0x3C) == 0x22, "RAM freeze did not survive reset");
        nes.insertCartridge(romPath.string());
        check(!cheats.isEnabled() && cheats.getEntries()[0].enabled, "Reload did not restore file defaults with master off");
        nes.unload();
        check(!cheats.isEnabled() && cheats.getEntries().empty(), "Unload retained cheat state");
        cheats.load(directory / "missing.cht");
        check(cheats.getEntries().empty(), "Missing file should yield empty list");
        cheats.load(argv[2]);
        const auto &entries = cheats.getEntries();
        check(entries.size() > 100, "Sample cheat file was not loaded");
        for (const auto &entry : entries)
            check(entry.code.empty() || entry.valid(), "Nonempty sample code failed validation");
        const auto energy = std::find_if(entries.begin(), entries.end(), [](const auto &entry)
                                         { return entry.code == "SEXEVGSZ"; });
        check(energy != entries.end(), "Sample energy cheat missing");
        cheats.setEntryEnabled(static_cast<size_t>(energy - entries.begin()), true);
        cheats.setEnabled(true);
        check(cheats.filterRead(0x8CAE, 0xA5) == 0x85, "Actual sample GG did not apply");
        std::cout << "Cheat tests passed; sample entries: " << entries.size() << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
