# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [0.7.5] - 2026-06-06

### Adicionado
- **Save & Load States**: Implementação de sistema de salvamento e carregamento instantâneo de estado com suporte a **3 slots independentes**.
- **Persistência de Estados**: Os estados são salvos em arquivos binários na pasta `/savestates`, permitindo continuar a jogatina mesmo após fechar o emulador.
- **Atalhos de Teclado**: Configuração de teclas rápidas para produtividade: `F5` para salvar e `F6` para carregar os respectivos slots.
- **Feedback Visual**: Notificações temporárias na console confirmando o sucesso do salvamento ou carregamento do estado.

## [0.7.0] - 2026-05-30

### Adicionado
- Implementação do **Mapper 003 (CNROM)**, adicionando suporte a clássicos como *Gradius*, *Arkanoid*, *Adventure Island* e *Tetris* (versão Nintendo).
- **Controle de Canais de Áudio**: Adicionada funcionalidade para habilitar/desabilitar individualmente os canais Pulse 1, Pulse 2, Triangle, Noise e DMC via menu Sound.
- **Sincronização de UI**: Melhoria de QoL onde os checkboxes dos menus (Sound e Debug) agora são atualizados corretamente para refletir o estado real das janelas e configurações ao serem acionados.
- Implementação de "User Mute" na APU, garantindo que a preferência do usuário no menu não seja sobrescrita por comandos do jogo.

## [0.6.0] - 2026-05-27

### Adicionado
- Implementação do **Mapper 002 (UNROM)**, expandindo a biblioteca para clássicos como *Castlevania*, *Contra*, *DuckTales* e *Mega Man*.
- Suporte funcional a **CHR-RAM**, permitindo que jogos sem CHR-ROM (como os de Mapper 2) carreguem seus próprios gráficos dinamicamente na memória da PPU.
- Refatoração do sistema de **Mirroring (Espelhamento)**: agora o Mapper tem autoridade para definir o modo de espelhamento, respeitando as configurações fixas de hardware definidas no cabeçalho iNES.

### Corrigido
- Renderização de **Sprites 8x16**: correção na seleção da Pattern Table baseada no bit 0 (LSB) do índice do tile, eliminando falhas visuais e sprites invisíveis.
- Correção crítica no Mapper 002 que impedia a escrita em CHR-RAM, resolvendo o problema de sprites ausentes em jogos de UNROM.

## [0.5.0] - 2026-05-26

### Adicionado
- Interface de **Overlay de Pause** visual utilizando ImGui, exibido centralizado quando a emulação está pausada, garantindo feedback claro especialmente em modo Fullscreen.
- Melhorias na estabilidade e latência do áudio: implementação de monitoramento de buffer (`SDL_GetQueuedAudioSize`) para evitar atrasos acumulados.
- Lógica de limpeza automática do buffer de áudio (`SDL_ClearQueuedAudio`) ao ativar o **Fast Forward** ou em situações de dessincronização (buffer > 50ms).
- Atualização dinâmica do título da janela para incluir o estado "PAUSED" e a identificação da build.

## [0.4.5] - 2026-05-24

### Adicionado
- Implementação da **APU (Audio Processing Unit)** v.Beta.
- Suporte aos canais: 2x Pulse, 1x Triangle, 1x Noise e esqueleto para DMC.
- Mixer não-linear implementado para maior fidelidade sonora.
- Integração de áudio via **SDL2** (amostragem de 44.1kHz, 32-bit Float).
- Sistema de sincronização de áudio por acumulador de ciclos e master volume gain (4.0x).
- Implementação do Frame Counter, Length Counters e Envelopes.

## [0.3.9] - 2026-05-20

### Adicionado
- Suporte inicial de visualização e edição da memória RAM em tempo real (propósito de debug)

## [0.3.8] - 2026-05-18

### Adicionado
- Suporte à emulação da **Zapper** (pistola de luz) utilizando o mouse.
- Lógica de detecção de brilho na PPU com área de sensibilidade de 5x5 pixels, simulando a lente da pistola e melhorando a precisão do clique.
- Mapeamento de hardware no Barramento (Bus) para os bits de gatilho e sensor de luz no endereço `$4017`.
- Sistema de conversão de coordenadas do mouse para o espaço de tela do NES (256x240), compatível com modos de janela e tela cheia (Stretch e 8:7).

## [0.3.7] - 2026-05-17

### Adicionado
- Implementação completa dos **Loopy's Registers** (`v`, `t`, `x`, `w`) para controle preciso de endereçamento de VRAM e renderização.
- Refatoração da comunicação da CPU com os registradores `$2000`, `$2005` e `$2006` para suportar o estado interno da PPU (latch de escrita e endereços temporários).
- Mecânica de incremento automático de Scroll (X e Y) e transferência de endereços sincronizada com o `clock()` da PPU nos ciclos específicos.

### Corrigido
- Problemas críticos de **Scroll Vertical** e wrapping de Nametables (corrigindo o posicionamento do HUD em *The Legend of Zelda*).
- Dessincronização visual onde os Sprites pareciam "deslizar" ou scrollar junto com o cenário.
- Eliminação de tremores (jitter) no scroll horizontal através do cálculo dinâmico de transbordo de tile usando `fineX`.
- Renderização de pixels do background agora utiliza o `vramAddr` atualizado para buscar dados de Pattern Table e Attribute Table com precisão de ciclo.

## [0.3.2] - 2026-05-09

### Adicionado
- Suporte parcial ao **Mapper 001 (MMC1)**, expandindo a compatibilidade para clássicos como *The Legend of Zelda*, *Metroid* e *Mega Man 2* (muitos bugs ainda)
- Implementação de suporte a arquivos comprimidos (**ZIP**), permitindo carregar ROMs diretamente sem necessidade de descompactação manual.
- Integração da biblioteca `zlib` no pipeline de build para descompressão de dados em tempo real.
- Sistema de persistência de configurações via `config.ini`, salvando o histórico das últimas 10 ROMs abertas.
- Memorização automática do último diretório de ROMs acessado, facilitando a navegação em diálogos de abertura de arquivo.

## [0.3.1] - 2026-05-08

### Adicionado (Sessão Atual)
- Implementação da fase de "Sprite Evaluation" no Ciclo 0 de cada scanline, otimizando drasticamente a performance da PPU.
- Suporte ao bit de Sprite Overflow ($2002 bit 5) e implementação fiel do limite de 8 sprites por linha.
- Recurso de "Unlimited Sprites" (overclocking de PPU) para remover o flicker clássico de sprites, com toggle em tempo real.
- Sincronização dinâmica de VSync entre a interface de usuário e o loop principal da Engine.
- Cache de índices de sprites por scanline para reduzir iterações no loop de renderização de pixels.
- Implementação de funcionalidade de Fast Forward (FF) acionada pela tecla TAB para aceleração dinâmica da emulação, controlada pela flag enable no menu

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
- Implementação de detecção de Sprite 0 Hit para sincronização de HUD.
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