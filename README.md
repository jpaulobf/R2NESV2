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
- **DPAD/Direcionais**: Setas do teclado ou D-Pad do controle.
- **A/B/Select/Start**: Teclas Z, X, Shift e Enter respectivamente.
- **Fast Forward**: Manter pressionado `TAB`.