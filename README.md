# R2NES V2

<div align="center">
  <img src="https://github.com/jpaulobf/R2NES/blob/main/nesemu%2F.assets%2Flogob.png"/>
</div>

R2NES V2 is a NES emulator written in C++17, using SDL3 and Dear ImGui. It runs on Windows and Linux and includes save states, battery saves, rewind, gamepad support, and debugging tools.

[Download releases](https://github.com/jpaulobf/R2NESV2/releases) · [Compatibility](COMPATIBILITY_LIST.md) · [Changelog](CHANGELOG.md) · [Test documentation](tests/README.md)

## Features

- CPU bus accesses distributed across instruction cycles, synchronized with the PPU.
- Background and sprite rendering, scrolling, Sprite 0 Hit, and selectable palettes and scanlines.
- Pulse, triangle, noise, and DMC audio, with individual channel controls.
- Keyboard, up to two gamepads, turbo buttons, and mouse-controlled Zapper.
- Three save-state slots, battery saves for supported boards, rewind with selectable precision, and fast forward.
- Game Genie and RAM cheats, with master and individual entry controls.
- CPU disassembler and viewers for RAM, VRAM, OAM, palettes, and pattern tables.
- iNES/NES 2.0 and supported UNIF images, loaded directly or from ZIP archives. UNIF is detected by content, including images named `.nes`.

Implemented mapper IDs: **0, 1, 2, 3, 4, 5, 7, 9, 11, 23, 40, 66, 71, 79, 90, 147, 150, 187, and 189**. Supported UNIF boards include `CNROM`, `SA-016-1M`, `TC-U01-1.5M`, and `Sachen-74LS374N`. Board variants and supported memory sizes vary; see the [test documentation](tests/README.md).

Recent optimizations reduce rewind allocations and copies, repeated CPU instruction classification, and per-dot PPU bookkeeping. Rewind presentation also avoids duplicate rendering within a loop iteration. Regression checks cover instruction cycles, open-bus decay, snapshots, and mapper timing; performance gains depend on the game and settings.

## Playing

Load a `.nes`, `.unf`, `.unif`, or `.zip` image from the **File** menu. Enable rewind, fast forward, or the Zapper from their menu options before using them. ROMs are not distributed with release packages.

| Action | Default keyboard control |
| --- | --- |
| Direction | W / S / A / D |
| A / B | K / J |
| Turbo A / Turbo B | I / U |
| Select / Start | Backspace / Enter |
| Fast forward | Hold Tab |
| Rewind | Hold Caps Lock |
| Pause | P or Pause |
| Save state / Load state | F5 / F6 |
| Window scale | F7–F10 |
| Borderless fullscreen | F11 |
| Reset | F12 |
| Zapper trigger | Left mouse button |

Gamepads support directional and action buttons, Start/Back, and turbo controls. Choose the save-state slot under **File → Save State Slot**, and enable the Zapper under **Input**.

Save states are stored in `savestates/`. Battery saves use `battery/<ROM CRC32>.sav`, relative to the working directory, and are written on ROM replacement, unload, or normal exit. Battery persistence covers mappers 1, 4, 5, and explicitly declared mapper 9 RAM extensions; standard Punch-Out!! has no battery. Read/write failures are shown in the interface.

## Compatibility

R2NES is actively developed. Mapper support does not guarantee that every game using it works correctly. CPU/PPU/APU timing and board coverage remain incomplete, and a small number of ROM-specific workarounds are still present.

- The current save-state format is `R2NK`; older snapshots are rejected. The recent performance optimizations preserve this format.
- Unlimited Sprites changes hardware behavior. Disable it when checking normal sprite limits and compatibility.
- Mapper 147 supports the 72-pin latch variant; the JV001 variant is not implemented. Mapper 150 supports the CPU D2 pad setting.
- Video currently uses SDL3's `SDL_Renderer`. Custom GPU shaders remain future work.

See the [compatibility list](COMPATIBILITY_LIST.md) for game observations and the [test documentation](tests/README.md) for board details, automated checks, and known limitations.

## Build from source

Requirements: **CMake 3.16+**, **GCC/G++ with C++17 support**, **Git**, and internet access for the first dependency download. CMake fetches SDL **3.4.18**, Dear ImGui **1.93.0**, and zlib **1.3.1**. SDL3 is linked statically with its GPU API enabled.

### Windows (MinGW-w64)

With GCC, G++, and `mingw32-make` on `PATH`, run from the repository root:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Linux

Install GCC/G++, CMake, Git, and the SDL3 development dependencies listed in the [release workflow](.github/workflows/release.yml), then run:

```bash
bash build-linux.sh
```

The script builds in `build-linux/`. Alternatively, configure and build manually:

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel
```

The executable and resources are placed in `bin/`. Keep the `resources/` directory beside the executable.

### Tests

Tests are enabled by default. After building, run:

```bash
ctest --test-dir build --output-on-failure
# For the Linux build directory:
ctest --test-dir build-linux --output-on-failure
```

The SDL integration test uses dummy audio/video drivers, software rendering, and virtual gamepads, so it can run without a desktop session. More details and individual test commands are in [tests/README.md](tests/README.md).

## Screenshots

![R2NES screenshot](screenshots/captura1.png)

![R2NES screenshot](screenshots/captura2.png)

![R2NES screenshot](screenshots/captura3.png)

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).
