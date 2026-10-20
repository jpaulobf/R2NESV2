#include "Window/Window.h"
#include "Window/ScopedImGuiContext.h"
#include "System/AudioManager.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

static SDL_Window *findWindow(const char *title)
{
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    SDL_Window *found = nullptr;
    for (int i = 0; i < count; ++i)
        if (std::strcmp(SDL_GetWindowTitle(windows[i]), title) == 0 && !(SDL_GetWindowFlags(windows[i]) & SDL_WINDOW_HIDDEN))
            found = windows[i];
    SDL_free(windows);
    check(found != nullptr, "Window not found");
    return found;
}

static void poll(Window &window)
{
    // SDL pode terminar um ciclo de polling antes dos eventos recém-enfileirados.
    for (int i = 0; i < 3; ++i)
        window.pollEvents();
}

static void pushKey(SDL_Window *window, SDL_EventType type, SDL_Keycode key)
{
    SDL_Event event{};
    event.type = type;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.key = key;
    event.key.scancode = SDL_SCANCODE_K;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    check(SDL_PushEvent(&event), "Could not push keyboard event");
}

static void testAudio()
{
    R2NES::System::AudioManager audio;
    check(audio.isInitialized(), "Could not initialize audio stream");
    check(audio.getSampleRate() == 44100, "APU input sample rate changed");
    SDL_ClearError();
    for (int i = 0; i < 44100; ++i)
        audio.pushSample(0.125f);
    audio.queueAudio();
    audio.pushSample(-0.125f);
    audio.queueAudio();
    audio.clearQueuedAudio();
    check(*SDL_GetError() == '\0', "Audio stream queue/clear failed");
    audio.close();
    audio.close();
    check(!audio.isInitialized(), "Audio stream survived close");
    audio.initialize();
    check(audio.isInitialized(), "Audio stream did not reopen");
}

static void testGamepads(Window &window)
{
    std::vector<int> buttons;
    std::vector<int> triggers;
    window.setControllerCallback([&](int player, SDL_GamepadButton button, bool pressed)
                                 {
                                     if (button == SDL_GAMEPAD_BUTTON_SOUTH && pressed)
                                         buttons.push_back(player); });
    window.setControllerTriggerCallback([&](int player, SDL_GamepadAxis axis, bool pressed)
                                        {
                                            if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER && pressed)
                                                triggers.push_back(player); });
    SDL_VirtualJoystickDesc description{};
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    description.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    description.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    description.name = "R2NES SDL3 test gamepad";
    std::array<SDL_JoystickID, 3> ids{};
    for (auto &id : ids)
    {
        id = SDL_AttachVirtualJoystick(&description);
        check(id != 0 && SDL_IsGamepad(id), "Virtual gamepad unavailable");
        poll(window);
    }
    for (auto id : ids)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        event.gbutton.which = id;
        event.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        event.gbutton.down = true;
        check(SDL_PushEvent(&event), "Could not push gamepad button");
        event = {};
        event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        event.gaxis.which = id;
        event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        event.gaxis.value = 20000;
        check(SDL_PushEvent(&event), "Could not push gamepad trigger");
    }
    poll(window);
    check(buttons == std::vector<int>({1, 2}), "Gamepad buttons reached wrong player");
    check(triggers == std::vector<int>({1, 2}), "Gamepad triggers reached wrong player");
    check(SDL_DetachVirtualJoystick(ids[0]), "Could not disconnect gamepad");
    poll(window);
    check(SDL_DetachVirtualJoystick(ids[1]), "Could not disconnect second gamepad");
    check(SDL_DetachVirtualJoystick(ids[2]), "Could not disconnect ignored gamepad");
    poll(window);
    window.setControllerCallback({});
    window.setControllerTriggerCallback({});
}

static void testViewers(Window &window)
{
    DebugWindowManager viewers;
    viewers.openTileViewer(0, 0, 256);
    viewers.openPaletteViewer(0, 0, 256);
    viewers.openRamViewer(0, 0, 256);
    viewers.openDisassembler(0, 0, 256);
    viewers.openOamViewer(0, 0, 256);
    viewers.openVramViewer(0, 0, 256);
    check(viewers.isTileViewerOpen() && viewers.isPaletteViewerOpen() && viewers.isRamViewerOpen() &&
              viewers.isDisassemblerOpen() && viewers.isOamViewerOpen() && viewers.isVramViewerOpen(),
          "A debug viewer failed to open");

    auto *context = ImGui::GetCurrentContext();
    std::array<uint8_t, 2048> memory{};
    std::size_t reads = 0;
    const MemoryAccess access{memory.size(),
                              [&](uint16_t address)
                              {
                                  check(address < memory.size(), "Viewer read outside memory");
                                  ++reads;
                                  return memory[address];
                              },
                              [&](uint16_t address, uint8_t value)
                              { memory.at(address) = value; }};
    viewers.updateRamViewer(access);
    viewers.updateVramViewer(access);
    check(reads == 4096, "Memory viewers did not read both memories");
    // Exercita o último bloco incompleto e o acesso somente leitura.
    const MemoryAccess partial{31, [](uint16_t address)
                               {
                                   check(address < 31, "Partial memory row exceeded size");
                                   return uint8_t{0x41};
                               },
                               {}};
    viewers.updateRamViewer(partial);
    viewers.updateVramViewer(partial);
    viewers.updateRamViewer({});
    viewers.updateVramViewer({});
    std::array<uint32_t, 128 * 128> tiles{};
    tiles.fill(0xFF336699);
    viewers.updateTileViewer(tiles.data(), tiles.data());
    std::array<uint8_t, 32> palette{};
    std::array<uint32_t, 64> colors{};
    viewers.updatePaletteViewer(palette, colors.data());
    std::array<uint8_t, 256> oam{};
    viewers.updateOamViewer(oam);
    bool stepping = false, requested = false;
    viewers.updateDisassembler(0x8000, {{0x8000, "NOP"}}, stepping, requested, 0, 0, 0, 0xFD, 0x24);
    check(ImGui::GetCurrentContext() == context, "Viewer changed main ImGui context");

    SDL_Window *main = findWindow("SDL3 integration");
    SDL_Window *ram = findWindow("R2NES v2 - RAM Viewer");
    int keys = 0;
    window.setKeyCallback([&](SDL_Keycode key, bool)
                          { if (key == SDLK_K) ++keys; });
    pushKey(ram, SDL_EVENT_KEY_DOWN, SDLK_K);
    pushKey(main, SDL_EVENT_KEY_DOWN, SDLK_K);
    pushKey(main, SDL_EVENT_KEY_UP, SDLK_K);
    poll(window);
    check(keys == 2, "Keyboard events crossed window boundaries");
    window.setKeyCallback({});

    check(viewers.closeWindow(SDL_GetWindowID(ram)) == DebugWindow::Ram, "Could not identify viewer close");
    check(!viewers.isRamViewerOpen(), "RAM viewer did not close");
    viewers.openRamViewer(0, 0, 256);
    check(viewers.isRamViewerOpen(), "RAM viewer did not reopen");
    viewers.closeAll();

    // Deixa viewers pertencentes à Window alocados para testar sua destruição antes de SDL_Quit.
    window.openVramViewer();
    window.openOamViewer();
    window.updateVramViewer(access);
    window.updateOamViewer(oam);
    SDL_Window *vram = findWindow("R2NES v2 - VRAM Viewer");
    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = SDL_GetWindowID(vram);
    check(SDL_PushEvent(&close), "Could not push viewer close");
    poll(window);
    check(!window.isVramViewerOpen(), "Viewer close event was ignored");
    check(!window.shouldClose(), "Closing viewer closed main window");
}

int main()
{
    try
    {
        {
            Window window("SDL3 integration", 256, 240, 1);
            check(!window.shouldClose(), "Main window initialization failed");
            testAudio();
            testGamepads(window);
            testViewers(window);
            window.setVSync(false);
            window.setVSyncSuppressed(true);
            window.setVSyncSuppressed(false);
            window.windowResize(2);
            std::array<uint32_t, 256 * 240> pixels{};
            pixels.fill(0xFF336699);
            window.render(pixels.data(), 60.0f);
            SDL_Event close{};
            close.type = SDL_EVENT_QUIT;
            check(SDL_PushEvent(&close), "Could not push quit");
            poll(window);
            check(window.shouldClose(), "Quit event ignored");
        }
        check(SDL_WasInit(0) == 0, "SDL survived window teardown");
        std::cout << "SDL3 integration passed (audio, viewers, contexts, keyboard, gamepads, teardown)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}