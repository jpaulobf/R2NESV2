#pragma once

#include "TestFixtures.h"
#include <set>
#include <sstream>
#include <stdexcept>

// Teste opcional: a ROM é fornecida localmente e não é distribuída pelo projeto.
static void smokeMulticart(const char *path, unsigned expectedMapper)
{
    using namespace R2NES::Core;
    FixtureNES nes("");
    nes.setCheatDirectory("");
    if (!nes.insertCartridge(path) || nes.getBus().cart->getMapperId() != expectedMapper)
        throw std::runtime_error("Multicart ROM or mapper rejected");
    const bool mapper58 = expectedMapper == 58;
    const unsigned frameLimit = mapper58 ? 900 : 360;
    unsigned frames = 0;
    std::string menuState;
    for (unsigned step = 0; step < (mapper58 ? 30000000u : 12000000u) && frames < frameLimit; ++step)
    {
        nes.step(false);
        if (!nes.isFrameComplete())
            continue;
        nes.clearFrameComplete();
        ++frames;
        if (frames == 120)
        {
            std::ostringstream state;
            nes.getBus().cart->getMapper()->saveState(state);
            menuState = state.str();
            nes.getJoysticks().controller1.setButton(IO::BUTTON_DOWN, true);
        }
        if (frames == 125)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_DOWN, false);
        if (frames == 180)
            nes.getJoysticks().controller1.setButton((mapper58 || expectedMapper == 51) ? IO::BUTTON_START : IO::BUTTON_A, true);
        if (frames == 185)
            nes.getJoysticks().controller1.setButton((mapper58 || expectedMapper == 51) ? IO::BUTTON_START : IO::BUTTON_A, false);
        if (mapper58 && frames == 600)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_A, true);
        if (mapper58 && frames == 605)
            nes.getJoysticks().controller1.setButton(IO::BUTTON_A, false);
    }
    std::ostringstream gameState;
    nes.getBus().cart->getMapper()->saveState(gameState);
    const auto *pixels = nes.getPpu().getFrameBuffer();
    const std::set<uint32_t> colors(pixels, pixels + 256 * 240);
    if (frames != frameLimit || colors.size() < 2 || menuState.empty() || menuState == gameState.str())
        throw std::runtime_error("Multicart did not show a selected bank with visible video");
    std::stringstream saved;
    if (!nes.saveState(saved))
        throw std::runtime_error("Multicart snapshot failed");
    const auto expected = saved.str();
    for (int i = 0; i < 10000; ++i)
        nes.step(false);
    saved.seekg(0);
    if (!nes.loadState(saved))
        throw std::runtime_error("Multicart restore failed");
    std::stringstream restored;
    if (!nes.saveState(restored) || restored.str() != expected)
        throw std::runtime_error("Multicart restore changed the snapshot");
    std::cout << "Multicart " << expectedMapper << ": " << frameLimit << " frames, selected bank, visible video and state restore; CRC "
              << nes.getBus().cart->getRomHash() << '\n';
}