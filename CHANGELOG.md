# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [0.1.0] - 2024-05-24

### Adicionado
- Estrutura inicial do projeto (Core Architecture).
- Sistema de build com CMake.
- Implementação do Barramento (Bus) com suporte a Mirroring.
- Esqueleto da CPU (6502) e abstração da RAM.

### Adicionado (Sessão Atual)
- Abstração completa de Cartucho separando `PRG-ROM`, `CHR-ROM` e `Mappers`.
- Implementação do suporte inicial a Mappers com o `Mapper 000` (NROM).
- Sistema de carregamento de arquivos `.nes` (iNES Format) com validação de cabeçalho.
- Evolução da CPU: Implementação real do ciclo de `Reset` buscando o vetor em `0xFFFC`.
- Configuração de Janela e Renderer via SDL2 com suporte a Texturas para saída da PPU.
- Primeira leitura bem-sucedida de dados de uma ROM real (SMB).
- Integração da biblioteca SDL2 para suporte a janelas, renderização e gerenciamento de eventos.
- Início da implementação da PPU: suporte aos registradores `$2006` (PPUADDR) e `$2007` (PPUDATA) com lógica de escrita dupla.
- Lógica de espelhamento de memória para Paletas e mascaramento de endereços de registradores ($2000-$3FFF).
- Implementação de Menu nativo do Windows (Win32 API) integrado ao loop de eventos do SDL2.
- Implementação funcional do Tile Viewer para visualização de CHR-ROM (Pattern Tables 0 e 1).
- Funcionalidade de "ímã" (magnet) para a janela Tile Viewer, fazendo-a seguir o movimento da janela principal automaticamente.
- Sistema de renderização multi-janelas (Multi-Window) para ferramentas de debug com Renderers independentes.
- Sistema de diálogo de abertura de arquivos (`GetOpenFileName`) para seleção dinâmica de ROMs.
- Arquitetura de Game Loop (`Engine`) com Fixed Timestep para sincronização de frames (60 FPS).
- Implementação da classe `VRAM` com suporte inicial a modos de Mirroring (Horizontal, Vertical, Four-Screen).
- Refatoração da arquitetura de barramento: PPU agora utiliza o `Bus` para acessar dados do cartucho, removendo o acoplamento direto com a classe `Cartridge`.
- Implementação inicial da lógica de `clock()` na PPU com suporte a flag de VBlank e contagem de scanlines/ciclos.
- Integração da biblioteca Dear ImGui para criação de interfaces de debug.
- Implementação do Disassembler em uma janela nativa independente utilizando ImGui e um pipeline de renderização dedicado.
- Integração de dados reais da CPU (PC e Disassembly) com a interface visual do Debugger.
- Sistema de realce (highlight) visual para a instrução atual no Disassembler.

### Corrigido
- Erros de definições múltiplas no header `Common.h` com a adição de Include Guards (`#pragma once`).
- Adição de FrameBuffer interno na PPU para armazenamento de pixels em formato ARGB.
- Incompatibilidade do sistema de build (CMake) com `ccache` e Response Files em ambiente Windows/w64devkit.
- Ajuste no roteamento de endereços da CPU para a PPU para garantir o espelhamento correto dos registradores.
- Erro de linkagem `undefined reference to WinMain` através da inclusão correta do alvo `SDL2::SDL2main` no CMake.
- Warning de redefinição da macro `NOMINMAX` no arquivo `Window.cpp`.
- Correção no processamento de eventos do SDL que impedia o fechamento da janela principal após o uso do Tile Viewer.
- Inicialização dos Renderers com cor preta para evitar artefatos visuais ("fundo branco") na criação das janelas.
- Garantia de `const`-correctness nos métodos de leitura de memória (PPU, Bus, Cartridge e VRAM).
- Bloqueio de carregamento para ROMs com Mappers ainda não suportados para evitar comportamento indefinido.
- Correção de crash (Assertion Failed) ao iniciar o emulador sem a janela de debug do ImGui inicializada.
- Proteção no loop de eventos para processar inputs do ImGui apenas quando a janela de debug estiver ativa.