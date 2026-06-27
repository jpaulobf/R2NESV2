# R2NES V2

<div align="center">
  <img src="https://github.com/jpaulobf/R2NES/blob/main/nesemu%2F.assets%2Flogob.png"/>
</div>

---
Still quite an early implementation of Nintendo Entertainment System (NES) emulation.
R2NES is a high-performance Nintendo Entertainment System (NES) emulator written in modern C++, focused on accuracy and development tools.

**Note**: This program contains no ROM or copyrighted data. You will need to provide your own ROMs to execute. Do not ask for ROMs.

**Note 2**: Builds are performed by the GitHub Action itself. If your system flags any behavior of malware, you can disable your antivirus. You can download, examine, and compile your own version anytime. The code is 100% opensource and free to use/copy.


## Current Features

- **CPU Core**: Full implementation of the 6502 (Ricoh 2A03).
- **PPU (Graphics)**: Cycle-accurate rendering with support for Loopy's Registers (v, t, x, w) for precise scrolling, Sprite 0 Hit, and Unlimited Sprites (flicker reduction).
- **APU**: Sound support with individual channel control (Pulse 1/2, Triangle, Noise, DMC) and non-linear mixing.
- **Mapper Support**: 
  - Mapper 000 (NROM) (100% perfect)
  - Mapper 001 (MMC1) * Still Buggy!! (in progress) Read the COMPATIBILITY_LIST.
  - Mapper 002 (UNROM) (almost perfect)
  - Mapper 003 (CNROM) (almost perfect)
  - Mapper 004 (MMC3) (initial support) * very buggy !
  - Mapper 007 (MMC3) (initial support) Battletoads works, but stage 2 crash after a while (use warpzone in stage 1)
  - Mapper 009 (HVC-PT) (100% perfect)
  - Mapper 040 (NTDEC) (100% perfect)
  - Mapper 066 (GNROM) (100% perfect)
  - Mapper 090 (GxROM) Buggy !!! (in progress)
- **File Formats**: Support for `.nes` (iNES) files and compressed `.zip` files.
- **Save States**: Support for 3 independent slots with disk persistence in the `/savestates` folder.
- **Debug Interface**: 
  - **Palette Viewer**: Real-time visualization and inspection of the PPU's Palette RAM.
  - **Tile Viewer**: Real-time visualization of Pattern Tables.
  - **Disassembler**: Assembly code debugging with execution highlighting.
- **Quality of Life (QoL)**:
  - **Real-time Palette Switching**: Ability to switch between different color palette presets (Default, Smooth, Nestopia, Wavebeam, Cyber Neon) on the fly.
  - **Recent Files**: Menu with history of the last 10 loaded ROMs for quick access.
  - **Directory Persistence**: Automatic memorization of the last opened folder, facilitating navigation.
- **Input**: Native support for Keyboard and Controllers (XInput/DirectInput via SDL2) with Hot-plugging.
- **Peripherals**: Zapper (Light Gun) emulation via mouse with precision mapping.

## Technologies Used

- **Language**: C++17
- **Graphics/Input**: SDL2
- **Debug UI**: Dear ImGui
- **Compression**: zlib
- **Build System**: CMake

## How to Build

1. Make sure you have `cmake` and a C++ compiler (GCC/MinGW recommended) installed.
2. Clone the repository.
3. In the project root, run:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```
4. The executable will be generated in the `/bin` folder.

## Default Controls
- **Directionals (D-Pad)**: `W`, `S`, `A`, `D` keys or controller D-Pad.
- **A / B**: `K` / `J` keys.
- **Turbo A / Turbo B**: `I` / `U` keys.
- **Select / Start**: `Backspace` / `Enter` keys.
- **Zapper (Light Gun)**: Left Mouse Click.
- **Fast Forward**: Hold down `TAB`.
- **Save State**: `F5`
- **Load State**: `F6`

---
## Screenshots:

<div align="center">
  <img src="https://github.com/jpaulobf/R2NESV2/blob/9f21708d45570f7b8cce75639e761914f46b0c84/screenshots/captura1.png"/>
</div>

---

<div align="center">
  <img src="https://github.com/jpaulobf/R2NESV2/blob/main/screenshots/captura2.png"/>
</div>

---

<div align="center">
  <img src="https://github.com/jpaulobf/R2NESV2/blob/main/screenshots/captura3.png"/>
</div>
