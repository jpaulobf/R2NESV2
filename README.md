# R2NES V2

<div align="center">
  <img src="https://github.com/jpaulobf/R2NES/blob/main/nesemu%2F.assets%2Flogob.png"/>
</div>

---
Still quite an early implementation of Nintendo Entertainment System (NES) emulation.
R2NES is a high-performance Nintendo Entertainment System (NES) emulator written in modern C++, focused on accuracy and development tools.

**Note**: This program contains no ROM or copyrighted data. You will need to provide the ROMs. Do not ask for ROMs.

**Note 2**: Builds are performed by the GitHub Action itself. If your system flags any behavior of malware, you can disable your antivirus. You can download, examine, and compile your own version anytime.


## Current Features

- **CPU Core**: Full implementation of the 6502 (Ricoh 2A03).
- **PPU (Graphics)**: Cycle-accurate rendering with support for Loopy's Registers (v, t, x, w) for precise scrolling, Sprite 0 Hit, and Unlimited Sprites (flicker reduction).
- **APU**: Sound support with individual channel control (Pulse 1/2, Triangle, Noise, DMC) and non-linear mixing.
- **Mapper Support**: 
  - Mapper 000 (NROM)
  - Mapper 001 (MMC1) * Still Buggy!! Read the COMPATIBILITY_LIST.
  - Mapper 002 (UNROM)
  - Mapper 003 (CNROM)
- **File Formats**: Support for `.nes` (iNES) files and compressed `.zip` files.
- **Save States**: Support for 3 independent slots with disk persistence in the `/savestates` folder.
- **Debug Interface**:
  - **Tile Viewer**: Real-time visualization of Pattern Tables.
  - **Disassembler**: Assembly code debugging with execution highlighting.
- **Quality of Life (QoL)**:
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
