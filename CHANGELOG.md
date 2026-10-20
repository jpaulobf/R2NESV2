# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

## [Unreleased]

- Corrigido o carregamento do dump legado de BB Car (CRC32 PRG+CHR `7F9C1DEC`): o mapper 7 do cabeçalho representa NROM com CHR-ROM e espelhamento fixo de uma tela. A identificação exata preserva a rejeição de CHR-ROM no AxROM normal. Validado boot, vídeo e save/load em `.nes` e no ZIP original; gameplay manual pendente.

### Adicionado

- Testes de regressão para o buffer de snapshots, os 256 opcodes da CPU em oito cenários por opcode e o decaimento do barramento de I/O da PPU, incluindo vencimentos independentes por bit e restauração de save states. Integrados ao CTest também os 48 casos existentes de sprite overflow.
- Testes de integração SDL3 com drivers de áudio/vídeo dummy, renderização por software e gamepads virtuais, permitindo execução sem sessão gráfica.
- Bateria integrada aos Mappers004 (MMC3 e MMC6), 005 e à extensão de RAM externa do Mapper009 declarada pelo cabeçalho. MMC6 persiste 1 KiB; MMC5 guarda PRG-NVRAM do primeiro chip e ExRAM interna, mantendo RAM volátil separada nas placas ETROM. No iNES MMC5 sem tamanho explícito, o superset de 64 KiB passa a separar 32 KiB persistentes e 32 KiB voláteis quando há bateria. 
- Política de gravação ao fechar/trocar/descarregar, sem autosave periódico. Suíte CTest `battery` com arquivos reais e novas sessões; validação manual pendente.
- Infraestrutura comum de bateria: contrato de memória persistente em `Mapper`/`Mapper.cpp`, leitura/gravação de arquivos em `BatterySave` e coordenação pelo cartucho/console. Primeiro mapper integrado: MMC1, incluindo Zelda, RAM/NVRAM explícita e segundo chip persistente de SOROM/SZROM. Arquivos binários `battery/<CRC32>.sav` são carregados ao abrir a ROM e gravados quando houver alterações ao trocar/descarregar o jogo ou fechar normalmente. Escrita temporária com substituição do destino, validação de tamanho e preservação de arquivos que falharam na leitura. Testes cobrem reinício do console, reset, bancos, save states, arquivos inválidos e falhas de gravação.
- Suporte ao Mapper150 (Sachen SA-015 / `Sachen-74LS374N`), com bancos PRG/CHR, oito registradores legíveis, bits de barramento aberto, save states e espelhamentos H/V, single-screen e em L. Implementado o pad com CPU D2 conectado; o pad alternativo ligado a Vcc permanece pendente.
- Carregamento UNIF de `NES-CNROM` pelo Mapper003 e de `UNL-Sachen-74LS374N` pelo Mapper150. Verificados os quatro ZIPs de *75 Bingo*, *Auto-Upturn* e dos dois cartuchos 2-in-1, cada um com 201 quadros, vídeo e restauração de save state.
- Carregamento UNIF das placas Sachen `SA-016-1M` e `TC-U01-1.5M`, incluindo arquivos com extensão `.nes` dentro de ZIPs e arquivos `.unf`/`.unif`. A identificação usa o conteúdo e o campo MAPR, preservando os arquivos originais.
- Suporte ao Mapper079 (NINA-03/06 / Sachen SA-016), com bancos PRG de 32 KiB, bancos CHR de 8 KiB, espelhamento H/V fixo e seleção sem conflitos de barramento. Permite carregar as versões Sachen de *Galactic Crusader* e *Master Chu & The Drunkard Hu*.
- Suporte ao Mapper147 (TC-U01-1.5M), variante de 72 pinos usada por *Challenge of the Dragon (Sachen)*, com latch de bancos PRG/CHR e conflitos AND nas escritas pelo espelho da PRG-ROM. A versão JV001 de 60 pinos e a configuração de 128 KiB de PRG não estão implementadas.
- Save states e suíte `sachen` no CTest para os Mappers079/147 e o parser UNIF, com verificações de bancos, endereços de registradores, ordenação e limites de chunks e rejeição de estados inválidos. Os três ZIPs Sachen avançaram 201 quadros com vídeo e restauração de estado nos testes de inicialização; partidas completas dependem de validação manual.

### Melhorado

- Migrada a interface, eventos, controles e saída de áudio para SDL3, com backends nativos do Dear ImGui. SDL3 é vinculada estaticamente; a saída de vídeo usa `SDL_Renderer`.
- Engine: captura de rewind escreve diretamente no buffer do snapshot e reutiliza memória dos estados removidos do histórico, reduzindo alocações e cópias. Removida uma consulta ao teclado cujo resultado não era usado.
- NES: aplicação de cheats de RAM é dispensada quando não há patches ativos; encaminhamentos de clock e IRQ do cartucho ficam disponíveis para otimização pelo compilador, preservando as chamadas ao mapper.
- CPU: classificação de branches, escritas e operações read-modify-write calculada ao construir a tabela de opcodes, reduzindo comparações de ponteiros de função a cada ciclo. A classificação é compartilhada entre as consultas de IRQ/NMI do mesmo ciclo.
- PPU: decaimento do barramento de I/O usa uma contagem até o próximo vencimento, atualizando os oito contadores quando necessário. Preservados os vencimentos por bit e o formato dos save states. Removidos controles de depuração sem uso do caminho de composição dos pixels.
- As otimizações de CPU e PPU foram comparadas com as versões anteriores em modos normal, overclock e saída de áudio suprimida, com coincidência dos estados completos, framebuffer e áudio nos cenários verificados. Os ganhos de desempenho dependem da ROM e do modo de execução.
- Documentação de apresentação simplificada, mantendo instruções para Windows/Linux e referências para compatibilidade, testes e detalhes dos mappers.
- Revisado o Mapper007 (AxROM) conforme o NesDev, com validação de memória e submappers, espelhamento físico dos bancos PRG e preservação das extensões de 16 KiB espelhados e 512 KiB.
- Ampliados e integrados ao CTest os testes do Mapper007, cobrindo conflitos de barramento, nametables pela PPU, CHR-RAM, save states e rejeição de imagens inválidas.
- Revisado o Mapper009 (MMC2), com validação de PRG/CHR e cabeçalhos e testes de regressão para bancos, latches, espelhamento, barramento aberto, depuração e save states.
- Revisado o Mapper011 (Color Dreams), com validação de memória e cabeçalhos, preservação das extensões de PRG de 16 KiB e CHR-RAM fixa, e testes integrados ao CTest.

### Corrigido

- Rewind passa a solicitar apresentação pelo caminho comum do loop, evitando duas renderizações e, com VSync ativo, duas esperas na mesma iteração.
- Save states `R2NK` preservam os cinco campos da DMA de OAM e conferem CRC32 da ROM e mapper antes de carregar componentes. Estados anteriores são rejeitados; arquivos de bateria mantêm seu formato.
- Bateria MMC5: importação explícita de layouts reconhecidos, com backup sem sobrescrever cópias anteriores, preenchimento de memória ausente com zeros e recusa de redução que descartaria dados distintos. Erros de leitura/gravação aparecem na interface; falhas ao gravar impedem trocar/descarregar a sessão com progresso novo. Dez suítes automatizadas passaram e o aplicativo compilou; validação manual dos diálogos pendente.
- Mapper005, segunda revisão (R1): inicialização e reset passam a usar os valores documentados `$5102=1`, `$5103=2` e `$5104=3`, mantendo WRAM protegida e ExRAM em leitura pela CPU, sem acesso como nametable até configuração explícita. Testes cobrem MMC5/MMC5A, preservação de memória e desbloqueio da WRAM. Mantido o formato `R2NI`; validação manual pendente.
- Mapper005, sétimo lote (item 14): revisão MMC5A explícita com timer de 16 bits, IRQ independente e controles digitais CL3/SL3; reset elétrico separado da inicialização completa, preservando os campos sem reset documentado. O carregador mantém MMC5 por padrão, sem inferir revisão pelo header. A detecção analógica da pausa em M2 e periféricos externos permanecem pendentes; áudio e bateria continuam adiados. Save states `R2NI` incluem revisão/GPIO/timer e rejeitam formatos anteriores. Testes cobrem IRQs simultâneas, DMA, GPIO, preservação no reset e restauração/rollback. Validação manual deste lote pendente; testes iniciais do lote 6 sem regressões relatadas.
- Mapper005 (MMC5), sexto lote (itens 12 e 13, sem persistência de bateria): capacidades PRG/CHR/WRAM validadas por cabeçalho, bancos e chips de WRAM mapeados com barramento aberto para chips ausentes, acesso aos 128 KiB e instalação de trainers sem liberar proteção. Registradores reconhecidos informam acesso interno, e o visualizador físico usa a CIRAM ativa do mapper. Adicionados testes de memória, cabeçalhos, truncamentos, trainers e restauração. O formato passa a `R2NH`, com WRAM dimensionada e configuração verificada; estados anteriores são rejeitados. Validação manual deste lote pendente.
- Mapper005 (MMC5), quinto lote (itens 9 e 11): validação semântica e carga atômica dos campos implementados, com testes de rejeição e rollback; split vertical por ExRAM com limites à esquerda/direita, scroll independente, paleta e banco CHR de 4 KiB na ligação CL comercial. A ligação SL e o pino 92 permanecem fora do escopo. Testes incluem pixels da PPU e restauração com split ativo. O formato passa a `R2NG`, rejeitando estados anteriores, inclusive `R2NF`. Áudio adiado para o final; validação manual deste lote pendente.
- Mapper005 (MMC5), primeiro lote da revisão: índices CHR de 2/4/8 KiB e captura individual dos bits de `$5130` em registradores internos de dez bits, preservando os bancos ao mudar o modo. Adicionados testes de bancos, transições, CHR-RAM/ROM e estados. Testes iniciais do usuário sem regressões relatadas.
- Mapper005 (MMC5), segundo lote: seleção CHR A/B por contexto de background, sprite e PPUDATA, observação de `$2000/$2001` exatos e atributos estendidos por tile com bancos de 4 KiB, paleta ExRAM e suporte à combinação com fill mode. O visualizador não altera a captura de atributos. Testes incluem leituras reais de `$2007` e pixels renderizados por ROM sintética; testes iniciais do usuário sem regressões relatadas.
- Mapper005 (MMC5), terceiro lote: linha IRQ derivada dos flags pendente/habilitado e contador de scanlines por pulsos reais de leitura da PPU, com primeira linha zero, detecção no dot 4 e timeout de três ciclos sem leituras. Incluídos os resets documentados por vetores NMI, PPUSTATUS, PPUMASK e OAMDMA, mantendo no relatório as ressalvas de confirmação em hardware do NesDev. Consultas de depuração não reconhecem IRQs. Testes verificam timing com a PPU real, Unlimited Sprites e restauração entre dots 3/4. O console grava `R2NF` e rejeita formatos anteriores, inclusive `R2NE`; testes iniciais do usuário sem regressões relatadas.
- Mapper005 (MMC5), quarto lote: permissões de ExRAM por modo e atividade de quadro, barramento aberto nas leituras da CPU em modos 0/1 e bloqueio de escrita por nametable em modos 2/3. Escritas da CPU em modos 0/1 fora de quadro são ignoradas como política determinística para o caso incerto documentado pelo NesDev. Ampliados testes de PPUDATA e de consultas de depuração, que preservam o estado completo do console, IRQ e detector parcial. Mantido o formato de estado `R2NF`; validação manual deste lote pendente.
- `MIRR=5` do UNIF é reconhecido como controle pelo mapper nas placas que o implementam. Modos incompatíveis com a placa e chunks duplicados continuam sendo rejeitados.
- Arquivos UNIF nomeados `.nes` das duas placas Sachen suportadas passam a ser reconhecidos pelo conteúdo, resolvendo a rejeição por assinatura iNES inválida. O CRC32 usa o payload PRG+CHR normalizado, sem depender dos metadados UNIF.
- Cartuchos Mapper007 com CHR-ROM, configurações de RAM não suportadas, submappers desconhecidos ou PRG truncada passam a ser rejeitados em vez de executados com memória incorreta.
- Save states do Mapper007 são validados antes de alterar bancos ou espelhamento, preservando os cinco bytes do formato anterior e recusando estados truncados ou corrompidos.
- Removida a PRG-RAM fictícia do Mapper009: a placa PxROM usa barramento aberto em `$6000–$7FFF`. Os estados antigos continuam sendo lidos, consumindo o espaço dessa memória sem mapeá-la na CPU.
- Escritas nos registradores do Mapper009 passam a ser reconhecidas como atendidas; estados inválidos ou truncados são rejeitados antes de alterar o mapper.
- O visualizador de Pattern Tables consulta o MMC2 sem disparar seus latches de CHR, evitando alterar o jogo durante a depuração.
- Implementados os conflitos AND de barramento da placa Color Dreams no Mapper011, usando o byte da PRG-ROM no endereço e banco anteriores à escrita. Variantes sem conflitos, como o protótipo *Free Fall*, ainda exigem identificação específica e podem regredir com essa política padrão.
- No Mapper011, escritas de registrador usam o sentinela de acesso interno; ROMs truncadas e configurações incompatíveis são rejeitadas, e save states inválidos falham sem alterar os bancos, mantendo o formato antigo de dois bytes.

## [1.0.5] - 2026-10-15

### Adicionado

- Suporte ao Mapper189, com seleção independente de bancos PRG de 32 KiB e lógica de CHR, espelhamento e IRQ do MMC3, incluindo persistência do banco PRG em save states.
- Suporte às variantes MMC6, espelhamento fixo, MC-ACC, NEC MMC3 e T9552 do Mapper004, selecionadas pelos submappers do NES 2.0, incluindo permissões de RAM específicas do MMC6 e memória adicional para espelhamento four-screen.
- Testes de regressão para os Mappers001, 002, 003, 004 e 189, com integração ao CTest e verificações com ROMs de teste de IRQ do MMC3. O teste do Mapper189 também permite verificar a inicialização de uma ROM ou ZIP informado pelo usuário.

### Melhorado

- Ampliado o suporte do Mapper001 às configurações de bancos e RAM das placas SNROM, SOROM, SUROM, SXROM e SZROM, além das variantes de PRG fixo e espelhamento pelo cabeçalho.
- Revisados os limites, o espelhamento físico de bancos e a seleção de CHR-ROM/CHR-RAM nos Mappers002 e 003, incluindo a RAM PRG de 2 KiB espelhada das placas Namcot no Mapper003.
- Reforçada a validação de cabeçalhos e tamanhos de memória dos Mappers001 a 004, com rejeição de configurações não suportadas e tratamento de trainers nas configurações compatíveis.
- Durante fast-forward, a APU deixa de misturar, filtrar e armazenar amostras de áudio, mantendo a execução dos canais, do sequenciador e do DMC.
- O loop principal usa espera por eventos entre os próximos quadros quando não está limitado pelo VSync, evitando consultas contínuas sem trabalho pendente. Os lotes de fast-forward retornam regularmente ao processamento de eventos.
- A captura de estados de rewind ocorre somente quando o recurso está habilitado; o histórico também respeita reduções de capacidade ao mudar a precisão.
- Atualizados o README, a lista de compatibilidade e a documentação dos testes com os recursos recentes, resultados observados e limitações conhecidas. Adicionados comentários em Português-BR nas alterações relevantes dos mappers e do núcleo.

### Corrigido

- Corrigida a tela preta na inicialização de *Street Fighter II: The World Warrior* com CRC32 `96CE586E`: esse dump iNES declara Mapper187 incorretamente e agora é carregado como Mapper189, sem alterar a ROM ou ZIP. A verificação de inicialização avançou 201 quadros com saída de vídeo; partidas completas ainda não foram validadas.
- Corrigidas a amostragem de IRQ e a captura de NMI na CPU, incluindo os efeitos de CLI, SEI, PLP, RTI e desvios. A linha de IRQ permanece sob controle da fonte que a gerou.
- Implementado o latch do barramento de dados da CPU para leituras de barramento aberto, incluindo RAM ausente ou desabilitada do cartucho; leituras de depuração não provocam os efeitos colaterais dos acessos normais.
- No Mapper001, o filtro de escritas consecutivas usa o contador de ciclos da CPU; o reset do registrador serial preserva os bits de espelhamento e modo de CHR.
- Corrigidos os conflitos de barramento dos Mappers002 e 003 conforme o submapper, a proteção contra escrita em CHR-ROM e a seleção de bancos conforme a memória disponível.
- No Mapper004, corrigidos os modos de bancos PRG/CHR, as permissões de RAM, o espelhamento e a recarga, habilitação e reconhecimento de IRQ, respeitando as diferenças entre as variantes.
- Ajustada a apresentação dos endereços da PPU ao mapper durante a renderização e os acessos a PPUADDR/PPUDATA, corrigindo a detecção e filtragem de A12 e clocks extras de IRQ do MMC3 entre linhas.
- Corrigido o tratamento da linha adicional de CHR do Mapper187 para preservar os acessos de nametable e o espelhamento four-screen compartilhados com a base MMC3.
- Reforçada a validação de save states dos Mappers001 a 004 e a restauração do console em caso de falha, evitando deixar componentes parcialmente carregados.
- Com Unlimited Sprites habilitado, o bit de sprite overflow continua sendo sinalizado ao encontrar o nono sprite, permitindo que jogos como *Bee 52* detectem essa condição. O comportamento não reproduz integralmente o bug de overflow do hardware.
- Os pulsos dos botões turbo usam um contador contínuo de quadros emulados, independente do contador de FPS, inclusive durante fast-forward.
- Pausas, carregamentos e resets limpam o áudio pendente e reiniciam os acumuladores de tempo, evitando reproduzir áudio antigo ou compensar o tempo gasto em pausas e diálogos.
- Carregar uma ROM ou save state, resetar ou descarregar o cartucho invalida o histórico anterior de rewind; desabilitar o recurso também encerra o retrocesso e limpa os estados capturados.

### Compatibilidade e limitações

- Os novos save states usam o formato `R2NC`, que inclui o contador de ciclos e o barramento de dados da CPU. A leitura de `R2NB` continua disponível, mas versões anteriores do emulador não leem `R2NC`.
- Estados antigos do MMC3 padrão mantêm compatibilidade; estados de variantes e de cartuchos four-screen precisam conter os campos adicionais exigidos pela configuração. A memória de bateria é incluída nos save states, mas ainda não há persistência automática em arquivos `.sav`.
- O glitch de recarga causado por escritas consecutivas em `$C001` no MMC3 ainda não é reproduzido. Os diagnósticos gerais de CPU/APU `4-jitter`, `6-irq_flag_timing`, `5-branch_delays_irq` e `2-nmi_and_brk` continuam apresentando falhas, também observadas na revisão anterior às correções do Mapper004. Detalhes em [tests/README.md](tests/README.md).

## [1.0.1] - 2026-10-12

### Adicionado

- Suporte ao Mapper071, incluindo espelhamento BF9097 de *Fire Hawk*, submapper 1 do NES 2.0 e detecção para cabeçalhos iNES antigos.
- Testes de regressão para os Mappers007 e 071, sprite overflow da PPU, espera por eventos SDL e supressão da saída de áudio da APU.

### Corrigido

- No Mapper007, conflitos de barramento são aplicados apenas ao submapper 2 do NES 2.0; cabeçalhos iNES antigos e submappers 0/1 usam seleção de bancos sem conflitos.
- Fast-forward suspende o VSync do renderer temporariamente e restaura o comportamento configurado ao soltar o atalho, preservando a preferência do menu.
- No modo Step-by-Step, a execução agora aguarda o fim do OAM DMA iniciado pela instrução antes de pausar no próximo limite de instrução.
- Implementado o latch de I/O da PPU, com decaimento aproximado por bit, para leituras de registradores write-only, bits baixos de PPUSTATUS e bits altos de leituras de paleta via PPUDATA.
- Modelado o bloqueio de escritas em PPUCTRL, PPUMASK, PPUSCROLL e PPUADDR durante a inicialização/reset da PPU; o contador é preservado em save states, cuja versão foi atualizada.
- Ajustado o timing do Sprite 0 Hit para sinalizar o flag de status um dot após a sobreposição de pixels opacos.
- Modelado o atraso de três dots dos bits de habilitação de rendering do PPUMASK, relevante para o alinhamento de scroll em *Battletoads* e *Battletoads & Double Dragon*.
- Removido o hack de Sprite 0 Hit de *Battletoads & Double Dragon* após aparentar estabilidade com o atraso de 3 dots; mantido temporariamente o fallback de *Battletoads*. O estado do hit pendente é preservado em save states.

## [1.0.0] - 2026-10-06

### Melhorado

- A execução da CPU passou da granularidade por instrução para ciclo a ciclo, distribuindo os acessos ao barramento pelos ciclos das instruções e avançando em sincronização com a PPU.

### Compatibilidade

- Adicionado workaround específico de Sprite 0 Hit para *Battletoads (USA)*, CRC32 `279710DC`, condicionado ao loop de espera identificado na ROM.
- Adicionado workaround específico de Sprite 0 Hit para *Battletoads & Double Dragon*, CRC32 `CEB65B06`, incluindo os loops de espera observados em `$8190` e `$8180`.
- Os workarounds são limitados às ROMs identificadas pelos hashes e não representam uma correção geral da sincronização CPU/PPU.

## [0.9.6] - 2026-09-27
- Refatorada a classe Window, reduzindo a complexidade e separando algumas responsabilidades (WIP)

## [0.9.5] - 2026-09-26

### Adicionado
- Suporte ao Mapper005 - Castlevania 3 funciona bem.
- Suporte à Rewind (CAPSLOCK por enquanto)

## [0.8.7] - 2026-06-27

### Adicionado
- Suporte ao Mapper011 - Quase todos os jogos funcionam (90%).
- Corrigidos ciclos do Mapper2
- Corrigidas Inconsistências na PPU
- Implementação inicial (WIP) de suporte a shaders

## [0.8.4] - 2026-06-27

### Adicionado
- Suporte ao Mapper009 - Punch-out funciona!

## [0.8.2.1] - 2026-06-25

### Adicionado
- Suporte a Full Crop para recorde de 8px em cada lado da tela (simulando o espaço oculto das CRTs)

## [0.8.2] - 2026-06-21

### Adicionado
- Suporte inicial ao Mapper007 - Battletoads funciona, mas crasha no estágio 2. Use o Warpzone da fase 1 para pular.

## [0.8.1] - 2026-06-20

### Melhorado
- Melhorias na classe Engine: Separação de Responsabilidades (pacote System foi criado)

### Adicionado
- Suporte ao Mapper040 - Super Mario Bros 2 The Lost Levels funcionando.

## [0.7.9] - 2026-06-13

### Adicionado
- **Inversão de Botões (QoL)**: Implementada a funcionalidade de inversão dos botões A/B para o layout X/A nos controles, facilitando a ergonomia para diferentes perfis de jogadores. O sistema agora realiza o remapeamento automático das funções de Turbo para os botões secundários correspondentes.

### Melhorado
- **Mapper 004 (MMC3)**: Diversas melhorias na lógica do Mapper 004, aumentando a compatibilidade com títulos que utilizam trocas complexas de bancos de memória e interrupções de IRQ.

## [0.7.8] - 2026-06-11

### Adicionado
- **Crop Overscan**: Adicionada a funcionalidade de ocultar as bordas superior e inferior (8px cada). O emulador agora recalcula dinamicamente o tamanho da janela e a proporção de aspecto (60:49 para o modo 8:7) para eliminar artefatos de scroll ("sujeira") sem distorcer a imagem.

### Corrigido
- **PPU**: Melhorias na estabilidade da renderização e correções de precisão no ciclo de quadros.
- **Visual Scanlines**: Correção na lógica de aplicação do efeito de scanlines para garantir o alinhamento dos pixels após o redimensionamento da janela.

## [0.7.7] - 2026-06-07

### Adicionado
- **VRAM Viewer**: Nova ferramenta de debug para visualizar e editar os 2KB de memória física das Name Tables em tempo real, com suporte a visualização ASCII.
- **OAM Viewer**: Visualizador para a Object Attribute Memory (OAM), permitindo inspecionar posição, índice de tile, paleta e atributos de todos os 64 sprites.
- **Mapper 066 (GxROM)**: Implementação do suporte para troca de bancos PRG (32KB) e CHR (8KB), habilitando jogos como *Super Mario Bros. + Duck Hunt*.
- **Mapper 090**: Implementação inicial do suporte ao Mapper 090.

### Melhorado
- **Precisão da CPU**: Refatoração do tratamento das flags de status (B e U) durante o empilhamento em interrupções (NMI, IRQ, BRK), alinhando o comportamento com o hardware real do 6502.
- **Interface de Debug**: Simplificação da janela de Disassembler para um visual mais limpo e atualização automática do cache de instruções ao detectar trocas de banco.
- **Arquitetura de Janelas**: Refatoração do método `Window::render` para separar a renderização principal da lógica de depuração, melhorando a performance e clareza do código.

## [0.7.6] - 2026-06-07

### Adicionado
- **Palette Viewer**: Nova ferramenta de debug para visualizar e inspecionar a Palette RAM da PPU em tempo real.
- **Seleção de Paletas**: Adicionada a funcionalidade de trocar entre diferentes presets de paletas de cores (Default, Smooth, Nestopia, Wavebeam, Cyber Neon) em tempo real.

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
