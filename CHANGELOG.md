# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [0.3.0] - 2026-05-07

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
- Implementação robusta de detecção de Sprite 0 Hit para sincronização de HUD.
- Sistema de renderização baseado em coordenadas absolutas (512x480) para scrolling estável.
- Funcionalidades de Reset e Unload de ROMs com limpeza de buffer de vídeo e estado do sistema.
- Automação de Unload antes de novos carregamentos de ROM para prevenir estados residuais.
- Suporte nativo a controles (Gamepads) via interface `SDL_GameController`.
- Sistema de Hot-plugging para detecção automática de conexão/desconexão de controles em tempo real.
- Implementação de mapeamento flexível de botões (Input Mapping) para até 2 jogadores.

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
- Correção de loop infinito (hang) no Super Mario Bros através da detecção de colisão do Sprite 0.
- Resolução de instabilidades no scroll (jitter) via sincronização de bits de Nametable no registrador $2006.
- Ajuste na lógica de wrapping vertical da PPU para respeitar o limite de 240 pixels (30 tiles).