# R2NES V2

<div align="center">
  <img src="https://github.com/jpaulobf/R2NES/blob/main/nesemu%2F.assets%2Flogob.png"/>
</div>

---

A high-performance Nintendo Entertainment System (NES) emulator written in modern C++, focused on accuracy and development tools.

## 🚀 Current Features

- **CPU Core**: Full implementation of the 6502 (Ricoh 2A03).
- **PPU (Graphics)**: Cycle-accurate rendering with support for Sprite 0 Hit and Unlimited Sprites (flicker reduction).
- **Mapper Support**: 
  - Mapper 000 (NROM)
  - Mapper 001 (MMC1)
- **File Formats**: Support for `.nes` (iNES) files and compressed `.zip` files.
- **Debug Interface**:
  - **Tile Viewer**: Real-time visualization of Pattern Tables.
  - **Disassembler**: Assembly code debugging with execution highlighting.
- **Quality of Life (QoL)**:
  - **Recent Files**: Menu with history of the last 10 loaded ROMs for quick access.
  - **Directory Persistence**: Automatic memorization of the last opened folder, facilitating navigation.
- **Input**: Native support for Keyboard and Controllers (XInput/DirectInput via SDL2) with Hot-plugging.

## 🛠️ Technologies Used

- **Language**: C++17
- **Graphics/Input**: SDL2
- **Debug UI**: Dear ImGui
- **Compression**: zlib
- **Build System**: CMake

## 📦 How to Build

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

## 🎮 Default Controls
- **Directionals (D-Pad)**: `W`, `S`, `A`, `D` keys or controller D-Pad.
- **A / B**: `K` / `J` keys.
- **Turbo A / Turbo B**: `I` / `U` keys.
- **Select / Start**: `Backspace` / `Enter` keys.
- **Fast Forward**: Hold down `TAB`.

---

# R2NES V2

Um emulador de Nintendo Entertainment System (NES) de alto desempenho escrito em C++ moderno, focado em precisão e ferramentas de desenvolvimento.

## 🚀 Funcionalidades Atuais

- **Core da CPU**: Implementação completa do 6502 (ricoh 2A03).
- **PPU (Gráficos)**: Renderização cycle-accurate com suporte a Sprite 0 Hit e Unlimited Sprites (redução de flicker).
- **Suporte a Mappers**: 
  - Mapper 000 (NROM)
  - Mapper 001 (MMC1)
- **Formatos de Arquivo**: Suporte a arquivos `.nes` (iNES) e arquivos comprimidos `.zip`.
- **Interface de Debug**:
  - **Tile Viewer**: Visualização das Pattern Tables em tempo real.
  - **Disassembler**: Depuração de código assembly com highlight de execução.
- **Qualidade de Vida (QoL)**:
  - **Arquivos Recentes**: Menu com histórico das últimas 10 ROMs carregadas para acesso rápido.
  - **Persistência de Diretório**: Memorização automática da última pasta aberta, facilitando a navegação.
- **Input**: Suporte nativo a Teclado e Controles (XInput/DirectInput via SDL2) com Hot-plugging.

## 🛠️ Tecnologias Utilizadas

- **Linguagem**: C++17
- **Graphics/Input**: SDL2
- **UI de Debug**: Dear ImGui
- **Compressão**: zlib
- **Build System**: CMake

## 📦 Como Compilar

1. Certifique-se de ter o `cmake` e um compilador C++ (GCC/MinGW recomendado) instalados.
2. Clone o repositório.
3. Na raiz do projeto, execute:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```
4. O executável será gerado na pasta `/bin`.

## 🎮 Controles Padrão
- **Direcionais (D-Pad)**: Teclas `W`, `S`, `A`, `D` ou D-Pad do controle.
- **A / B**: Teclas `K` / `J`.
- **Turbo A / Turbo B**: Teclas `I` / `U`.
- **Select / Start**: Teclas `Backspace` / `Enter`.
- **Fast Forward**: Manter pressionado `TAB`.