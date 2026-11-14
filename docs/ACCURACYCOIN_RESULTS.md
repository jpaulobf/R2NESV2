# AccuracyCoin: execução headless

## ROM principal de 147 testes

Em 2026-11-04, a ROM fornecida em `roms/AccuracyCoin.nes` foi comparada à
versão do repositório principal do AccuracyCoin: os arquivos têm o mesmo
SHA-256 `9332c3735dd7dea2f4711e670d6478d8b0085edeacdd4f9f300fb04a8bbe3f88`.
Essa versão contém 147 testes, ao contrário
do fork mais antigo relatado abaixo.

O usuário executou a aplicação com interface e obteve **92/147**. Uma
execução headless posterior com a mesma ROM e o núcleo compilado localmente
atingiu a tela de resultados e registrou **93/147** nos contadores `$37` e
`$38`. O motivo da diferença de um teste não foi identificado; podem existir
diferenças de configuração ou de execução. Nenhuma causa está estabelecida.
O executor continuou até o limite programado após a tela de resultados; a
mensagem `TIMEOUT` nesse log não significa que a suíte ficou presa.

Na execução headless, `$BB LAE` (LAS) retornou PASS. O resultado bruto e o
executor temporário estão em `build-headless/accuracycoin/`. A comparação
entre as duas versões da ROM não é uma comparação direta de pontuações.

Após as correções de open bus dos controles e da leitura de `$4015`, a mesma
execução headless retornou **94/147**: Open Bus mudou de FAIL código 6 para
PASS, e nenhum teste que passava passou a falhar. A referência anterior de
93/147 está em `tests/accuracycoin-baseline.tsv`; o executor reproduzível e
os comandos estão descritos em `tests/README.md`. Internal Data Bus e Implied
Dummy Reads ainda falham em pré-requisitos envolvendo DMC DMA.

Após a primeira correção da janela de NMI sobre BRK/IRQ, a execução ainda
resultou em **94/147**, sem regressões PASS/FAIL frente à referência original.
As três verificações de interrupção continuam falhando: Interrupt Flag
Latency (A), NMI Overlap BRK (2) e NMI Overlap IRQ (1). A correção passou em
testes sintéticos de ciclos, mas não resolve o timing completo da suíte.

Em 2026-11-06, após ajustar a contagem da escrita em `$4017` e o ciclo zero
do sequenciador da APU, a execução headless passou para **96/147**. Length
Counter e Length Table passaram; não houve regressões PASS/FAIL frente aos
94/147 anteriores. Frame Counter 4-step e Frame Counter IRQ ainda falham,
portanto o resultado não estabelece precisão completa do frame counter.
O resultado detalhado está em `build-headless/accuracycoin/reset-experiment.txt`.
A suíte CTest headless também passou: 34/34 testes.

Na correção seguinte, a saída de strobe passou a ser amostrada na fase do
relógio do sistema, e **Controller Strobing passou**. A ROM completou com
**97/147**, sem regressões PASS/FAIL frente aos 96/147 anteriores; Controller
Clocking continua PASS. O resultado está em
`build-headless/accuracycoin/after-controller.txt`. CTest headless: 34/34.

### Próxima investigação: DMA do DMC

Antes das alterações desta rodada, o código lia a amostra imediatamente e só
depois contabilizava os ciclos de espera; também deixava a DMA de OAM
prosseguir sem arbitragem com o DMC. A ROM acusa `DMA + Open Bus` código 2: o primeiro teste
passa, mas o seguinte não observa o valor da leitura DMC no barramento no
momento esperado. `DMC Playback Rate` código 1 e `DMA Landing on Write`
código 2 dependem de `DMA + Open Bus`; passar no pré-requisito não garante que
as verificações seguintes passarão.

Uma implementação precisa distinguir os ciclos de tentativa de parada,
dummy, alinhamento e leitura, adiar a parada durante escritas da CPU e
arbitrar com OAM DMA. As leituras repetidas podem ter efeitos colaterais;
estados intermediários precisam sobreviver a save/load. Recomenda-se começar
com testes sintéticos desses ciclos e do barramento, antes de tratar conflitos
com registradores e cancelamentos. O timing de VBlank/NMI continua sendo um
grupo independente para uma rodada posterior.

Primeira etapa implementada: a CPU identifica seu próximo ciclo de escrita e
o DMC aguarda até um ciclo de leitura antes de iniciar a transferência. Uma
verificação sintética cobre uma requisição antes das duas escritas de `INC`;
outra compara a classificação com o acesso real em cenários dos 256 opcodes.
Após essa mudança, a ROM continua em **97/147**, sem regressões PASS/FAIL;
`DMA + Open Bus` permaneceu no código 2. Nessa primeira etapa, a leitura DMC
ainda ocorria antes dos ciclos de espera, sem arbitragem DMC/OAM.
O resultado está em `build-headless/accuracycoin/dmc-write-halt.txt`.

Na etapa seguinte, o caminho NTSC passou a distinguir parada, ciclo dummy,
alinhamento e leitura, e a leitura DMC tem prioridade sobre a leitura de OAM.
Um teste sintético cobre a sobreposição e a restauração durante a DMA. A ROM
continua em **97/147**, sem regressões PASS/FAIL; `DMA + Open Bus` (2) e
`DMC DMA + OAM DMA` (1) ainda falham. O agendamento dos pedidos e os conflitos
com registradores, entre outros detalhes, ainda exigem investigação. O caminho
PAL conserva a aproximação anterior. CTest headless: 34/34; resultado completo
em `build-headless/accuracycoin/dmc-phased.txt`.

### VBlank

Uma leitura de `$2002` imediatamente antes da borda de VBlank agora suprime
o levantamento da flag e a NMI daquela borda, sem impedir a conclusão do quadro.
Em 2026-11-06, `VBlank beginning` passou e a ROM terminou em **98/147**, sem
regressões PASS/FAIL frente aos 97/147 anteriores. Os testes de timing e
supressão de NMI ainda falham. O resultado completo está em
`build-headless/accuracycoin/after-vblank.txt`; CTest headless: 34/34.

### Escritas não oficiais

SHA, SHS/TAS, SHY e SHX agora usam o byte alto do endereço base mais um
para calcular o valor escrito. Quando o índice cruza uma página, o valor
também substitui o byte alto do endereço de escrita, seguindo o comportamento
da revisão inicial descrita pela própria ROM. Testes sintéticos cobrem as
cinco variantes com e sem cruzamento de página.

O AccuracyCoin permanece em **98/147**, sem regressões PASS/FAIL. Os cinco
testes avançaram do código 1 para o código 7, nas verificações envolvendo
DMA, que continuam pendentes. Resultado em
`build-headless/accuracycoin/after-unstable-stores.txt`; CTest: 35/35.

### Fase de recarga do DMC e parada da DMA de OAM

Em 2026-11-06, a execução passou de **98 para 109/147**, sem regressões
PASS/FAIL frente aos 98 anteriores. As recargas do DMC agora solicitam a
parada na fase put do relógio NTSC; antes podiam começar em get e consumir
três ciclos em vez dos quatro esperados. Isso também impedia a rotina de
sincronização da ROM de observar a leitura DMC no barramento. O caminho PAL
de recarga mantém a aproximação anterior.

A DMA de OAM também aguarda as escritas pendentes da CPU antes de parar.
Assim, uma instrução RMW em `$4014` completa as duas escritas e a segunda
seleciona a página transferida. Essa correção é aplicada em NTSC e PAL.

Os onze novos PASS são Interrupt flag latency, DMA + Open Bus, DMA + $2007
Read, DMA + $2007 Write, DMA + $4015 Read, DMA + $4016 Read (código informativo
2), DMC DMA + OAM DMA, Instruction Timing, Implied Dummy Reads, DMC Playback
Rate e INC $4014. O log completo está em
`build-headless/accuracycoin/dmc-reload-put.txt`. O campo `improved 16` desse
log compara com a referência original de 93/147.

Os testes sintéticos cobrem a cadência de recarga nas 16 taxas NTSC e nas
duas fases iniciais, os quatro ciclos de parada durante uma recarga, a
retomada da CPU e as duas escritas de INC $4014 em NTSC/PAL. A suíte de
save/load continua cobrindo restauração durante DMA e sobreposição DMC/OAM.
CTest headless: **35/35**. O atraso inicial após `$4015`, cancelamentos, conflitos de barramento e os
detalhes PAL ainda exigem trabalho; os PASS não estabelecem precisão completa.
Referência de fases e parada: [NESdev DMA](https://www.nesdev.org/wiki/DMA).

### Carga inicial e conflitos de barramento do DMC

Em 2026-11-07, a primeira transferência DMC NTSC passou a aguardar três ou
quatro ciclos após a escrita em `$4015`, conforme a fase, antes de tentar
parar a CPU. No get DMC com a CPU parada em `$4000-$401F`, os registradores
internos `$4015-$4017` também respondem quando selecionados pelos cinco bits
baixos do endereço da amostra. A leitura da amostra continua chegando ao DMC;
os efeitos dos registradores e o valor observado no barramento externo são
tratados no mesmo ciclo.

O AccuracyCoin terminou em **111/147**, sem regressões PASS/FAIL frente aos
109/147: passaram `DMA Landing on Write` (código informativo 1) e `DMC DMA
Bus Conflicts`. O resultado está em `build-headless/accuracycoin/dmc-conflicts.txt`.
O teste sintético cobre a carga inicial nas duas fases, parada e retomada da
CPU, e conflito com `$4016`, incluindo um caso sem conflito. A restauração
durante a espera inicial e a rejeição dos formatos anteriores também são
testadas. A suíte CTest headless passou 35/35.

Os save states são `R2NU` (NTSC) e `R2NV` (PAL). Apenas esses formatos são
aceitos; versões anteriores não leem os novos estados.

Os cancelamentos de DMA, conflitos durante OAM, detalhes de áudio durante
conflitos e o comportamento PAL do DMC ainda exigem investigação. `DMA +
$2002 Read` continua falhando. Referências: [NESdev: DMA](https://www.nesdev.org/wiki/DMA)
e [pesquisa sobre os registradores internos durante DMA](https://forums.nesdev.org/viewtopic.php?t=23543).

### DMA durante escritas não oficiais

Em 2026-11-08, o AccuracyCoin passou de **111 para 116/147**, sem regressões
PASS/FAIL. Os cinco novos PASS são SHA indireto,Y, SHA absoluto,Y, SHS/TAS,
SHY e SHX. Quando a parada DMC ocorre na leitura indexada imediatamente antes
da escrita, o valor deixa de ser mascarado pelo byte alto do endereço base;
com cruzamento de página, a alteração do endereço continua usando a máscara.
As condições seguem o comportamento de CPU inicial já escolhido pela suíte;
as instruções são instáveis e outros chips podem diferir. Referência:
[NESdev: instruções não oficiais](https://www.nesdev.org/wiki/Programming_with_unofficial_opcodes).

Os testes cobrem as cinco instruções com e sem cruzamento de página, DMA no
ciclo relevante, DMA um ciclo antes e ausência de DMA. Também verificam a
restauração durante a parada e a rejeição com rollback de estados corrompidos.
O indicador da parada usa um campo já serializado da CPU, mantendo o formato
de save state. Um estado salvo por uma versão anterior exatamente durante
essa interrupção rara não contém o indicador e pode retomar com o resultado
antigo. Log completo: `build-headless/accuracycoin/dmc-unstable-stores.txt`.

### Sequenciador de quadro e IRQ da APU

Em 2026-11-08, o AccuracyCoin passou de **116 para 119/147**, sem regressões
PASS/FAIL. Passaram `Frame Counter 4-step`, `Frame Counter IRQ` e `DMA + $2002
Read` (código informativo 1). O sequenciador de quatro passos reinicia um
ciclo após o último clock, evitando adiantar as sequências seguintes.

Em NTSC, a leitura de `$4015` agenda a limpeza da flag de IRQ para a próxima
transição de put para get. A flag pode ser reafirmada durante os três ciclos
do último passo e fica temporariamente visível nos dois primeiros mesmo com a
IRQ inibida, sem acionar a linha de interrupção. Esse ajuste também permite que
a ROM sincronize corretamente o teste `DMA + $2002 Read`.

Os testes sintéticos verificam a cadência de length counter em NTSC e PAL,
as fases de leitura, a reafirmação da flag, a inibição e a restauração da
limpeza pendente, inclusive no estado completo do console. O formato atual de
save state mantém o mesmo tamanho; versões anteriores podem rejeitar um
estado salvo exatamente com a limpeza pendente. O comportamento PAL dessa
limpeza e da indicação com IRQ inibida não foi validado nesta rodada.
Resultado: `build-headless/accuracycoin/frame-irq-inhibit.txt`. Referência:
[NESdev: timing da IRQ da APU](https://forums.nesdev.org/viewtopic.php?t=635).

### Fase inicial do temporizador DMC

Em 2026-11-11, o AccuracyCoin passou de **119 para 120/147**: `DMC Reload
Timing` passou, sem regressões PASS/FAIL. A fase inicial NTSC do temporizador
DMC foi atrasada em um ciclo. Isso permite que uma escrita em `$4010` no ciclo
da primeira recarga forneça o novo período, sem alterar a cadência entre
recargas posteriores. A implementação PAL permanece na fase anterior, sem
afirmação de equivalência com o hardware 2A07.

O teste `apu-dmc-reload` verifica escritas em `$4010` no ciclo da primeira
recarga e no ciclo seguinte, observando quando o buffer esvazia e solicita a
DMA. O teste `cpu-bus` continua cobrindo as 16 taxas NTSC nas duas fases e a
parada da CPU. Resultado completo:
`build-headless/accuracycoin/dmc-reload-phase.txt`. Referências:
[NESdev: APU DMC](https://www.nesdev.org/wiki/APU_DMC) e
[NESdev: DMA](https://www.nesdev.org/wiki/DMA).

### Cancelamento de DMA do DMC

Em 2026-11-11, o AccuracyCoin avançou de **120 para 122/147**, sem regressões
PASS/FAIL: passaram `Explicit DMA Abort` e `Implicit DMA Abort` (este último
com código informativo 1, correspondente ao comportamento do RP2A03G tardio).
Desligar `$4015.4` não apaga uma transferência já agendada. Se a amostra
termina um ou dois ciclos antes da tentativa de parada, a DMA pode parar a CPU
por apenas um ciclo; quando essa tentativa coincide com uma escrita da CPU, ela
é descartada. No caso implícito, uma amostra de um byte sem loop pode gerar
essa parada ou uma leitura extra do mesmo endereço, conforme a fase.

O teste `dmc-abort` cobre a transferência completa após stop, a parada
abortada, a escrita que a cancela, os dois resultados da parada implícita e a
restauração dos estados armado e pendente. Esses estados usam bits livres do
byte serializado do atraso de carga DMC, sem alterar o tamanho do save state;
uma versão anterior pode rejeitar um estado salvo nesses instantes raros.
O comportamento PAL permanece pendente. Resultado completo:
`build-headless/accuracycoin/dmc-abort.txt`. Referência:
[NESdev: DMA do DMC e seus bugs](https://www.nesdev.org/wiki/DMA#Bugs).

### Ativação de registradores durante a DMA de OAM

Em 2026-11-11, o AccuracyCoin avançou de **122 para 123/147**, sem regressões
PASS/FAIL. `APU Register Activation` passou com código 2. Os bits 15–5 do
endereço no qual a CPU foi parada controlam a ativação dos registradores
internos; os bits 4–0 vêm da fonte da DMA. Assim, uma fonte `$50xx` pode ler
`$4015–$4017` enquanto a CPU está parada em `$4000–$401F`, e uma fonte
`$40xx` não os ativa fora dessa janela. Quando a RAM dirige o barramento,
o controle ainda avança, mas o byte copiado vem da RAM. O put da DMA atualiza
a trava externa de dados. Os bits 2–4 dos atributos da OAM primária agora
voltam como zero.

O teste `oam-dma-register` cobre a janela de ativação, limpeza da IRQ de
quadro, conflito com RAM, avanço do controle, atributos da OAM e barramento
após o put. O teste de restauração de estado foi atualizado para comparar
os atributos efetivamente armazenados. `Address $2004 behavior` continuou
FAIL, mas seu código mudou de 3 para A; as outras 145 linhas mantiveram o
mesmo código. Resultado completo:
`build-headless/accuracycoin/oam-register-activation.txt`. Referências:
[NESdev: DMA](https://www.nesdev.org/wiki/DMA) e
[NESdev: PPU OAM](https://www.nesdev.org/wiki/PPU_OAM).

### Investigação de $2004 após 123/147

Uma experiência em 2026-11-12 zerou os dois bits baixos de OAMADDR em toda
escrita de `$2004` durante a renderização. O AccuracyCoin então retornou
**124/147**, com `Address $2004 behavior` PASS e nenhuma regressão PASS/FAIL
frente aos 123 anteriores. Essa alteração foi descartada: aplicar o alinhamento
indistintamente contraria a descrição usual do incremento dos seis bits altos
de OAMADDR durante a renderização. O resultado experimental, por si só, não
valida o comportamento nas demais fases.

No ponto que distinguiu as duas implementações, a instrumentação observou a
escrita com OAMADDR igual a 1 no ciclo 0 da scanline 0; o comentário do código
da ROM situa a escrita no dot 321 da linha de pré-renderização. Essa diferença
merece investigação do sincronismo entre a ROM, os acessos da CPU e a PPU,
antes de decidir como modelar o alinhamento. O núcleo continua com o comportamento
anterior; naquela experiência, a pontuação confirmada permaneceu **123/147**. Log da experiência:
`build-headless/accuracycoin/oam-align-experiment.txt`. Referência:
[NESdev: registradores da PPU](https://www.nesdev.org/wiki/PPU_registers#OAMDATA).

### Reativação do DMC com buffer cheio

Em 2026-11-12, o AccuracyCoin avançou de **123 para 124/147**: `Delta
Modulation Channel` passou, e as outras 146 linhas conservaram seus resultados
e códigos. Em NTSC, quando `$4015` reinicia uma amostra e ainda há um byte no
buffer, a DMA também deve aguardar a janela de ativação de três ou quatro
ciclos, conforme a fase. Se o buffer esvazia nessa janela, a solicitação de
recarga fica pendente até a ativação. Isso cobre as últimas três verificações
da ROM, em que o buffer esvazia próximo à escrita de reativação.

O teste `apu-dmc-reload` cobre três instantes de reativação, as duas fases e a
restauração antes e depois de a recarga ficar pendente. O campo de atraso já
serializado passou a poder existir antes da solicitação; o tamanho do save state
não mudou, mas versões anteriores podem rejeitar estados salvos nessa janela.
O comportamento PAL não foi alterado nem validado nesta rodada. CTest headless:
**47/47**. Resultado: `build-headless/accuracycoin/dmc-buffered-restart.txt`.
Referência: [NESdev: DMA do DMC](https://www.nesdev.org/wiki/DMA#DMC_DMA).

### Pipeline de fundo com apenas sprites habilitados

Em 2026-11-12, o AccuracyCoin avançou de **124 para 125/147**: `Rendering
Flag Behavior` passou; as outras 146 linhas mantiveram resultados e códigos.
O pipeline de fundo agora avança os registradores de
deslocamento quando qualquer uma das duas camadas está habilitada. O bit de
fundo continua determinando se seus pixels aparecem na imagem, e desligar as
duas camadas continua suspendendo o pipeline.

O teste `ppu-sprite-overflow` compara os pixels após reativar o fundo no meio
da linha com uma PPU que manteve o fundo habilitado; a variante antiga falha
nessa comparação. O teste também mantém os casos de avaliação de sprites com
ambas as camadas separadamente habilitadas. CTest headless: **47/47**.
Resultado completo: `build-headless/accuracycoin/ppu-rendering-flags.txt`. Referência:
[NESdev: registradores e habilitação da renderização](https://www.nesdev.org/wiki/PPU_registers).

### Barramento interno da CPU

Em 2026-11-13, o AccuracyCoin avançou de **125 para 126/147**: `Internal
Data Bus` passou e as outras 146 linhas conservaram seus resultados e códigos.
O bit 5 aberto de `$4015` agora usa uma trava interna própria. Leituras e
escritas da CPU atualizam essa trava, enquanto o get DMC NTSC altera o
barramento externo sem alterar a trava interna. Leituras de `$4015` continuam
sem alterar a trava externa.

Os testes verificam a independência das duas travas, leituras de depuração e
restauração de um estado em que elas diferem. A ROM exercita a transferência
DMC real. CTest headless: **48/48**. Resultado completo:
`build-headless/accuracycoin/internal-data-bus.txt`. O modelo PAL de DMA e
interações adicionais durante DMA de OAM não foram validados nesta rodada.

Os save states agora usam `R2NW` (NTSC) e `R2NX` (PAL), com o bit interno
guardado no byte de estado da DMA de OAM. Os formatos anteriores são rejeitados
porque não guardam essa informação; versões anteriores não leem o novo formato.

### Início da contagem X dos sprites

Em 2026-11-13, o AccuracyCoin avançou de **126 para 127/147**: `Stale BG
Shift Registers` passou e as outras 146 linhas conservaram seus resultados e
códigos. Quando ambas as camadas estão desligadas no dot 339, as unidades de
sprite preparadas para a linha seguinte passam a emitir imediatamente ao
reativar a renderização, em vez de esperar pela coordenada X carregada.

O teste `ppu-sprite-overflow` verifica sprites com X igual a 128 nas duas
condições do dot 339 e restauração após esse dot. CTest headless: **48/48**.
O formato de save state não mudou. A correção cobre essa condição específica;
o avanço dos shifters quando a renderização é desligada durante uma linha e
outras particularidades dos sprites ainda não estão integralmente modelados.
Resultado completo: `build-headless/accuracycoin/sprite-339-experiment.txt`.
Referência: [pesquisa sobre renderização de sprites no NESdev](https://forums.nesdev.org/viewtopic.php?t=26291).

### Mudança do tamanho do sprite durante a busca

Em 2026-11-13, o AccuracyCoin avançou de **127 para 128/147**: `Suddenly
Resize Sprite` passou e as outras 146 linhas mantiveram seus resultados e
códigos. Um sprite pode ser selecionado enquanto o tamanho é 8×16 e ter seu
padrão buscado depois de uma mudança para 8×8. Nesse caso, o endereço do
padrão usa apenas os três bits baixos da linha, antes da inversão vertical.

O teste `ppu-sprite-overflow` verifica a mudança antes e depois da busca do
primeiro plano do padrão, com e sem inversão vertical. A implementação anterior
falha no teste novo. CTest headless: **48/48**. O formato de save state não
mudou. Resultado completo: `build-headless/accuracycoin/sprite-resize-experiment.txt`.
Referência: [NESdev: sinais da PPU](https://www.nesdev.org/wiki/PPU_signals).
O resultado não valida todas as mudanças de PPUCTRL durante buscas parciais.

### Amostragem de NMI entre CPU e PPU

Em 2026-11-13, o AccuracyCoin passou de **128 para 131/147**: `NMI Overlap
BRK`, `NMI Overlap IRQ` e `NMI Timing` passaram. As outras 144 linhas mantiveram
seus resultados e códigos. A comparação das tabelas de interrupção mostrou
um deslocamento comum; em NTSC, a NMI passou a ser entregue à CPU após o
primeiro dos três clocks de PPU do passo, em vez de após o terceiro. Bordas
geradas nos dois últimos ficam pendentes até a amostragem seguinte.

O teste `cpu-interrupt` cobre as três fases de VBlank, a entrega da NMI e
save/load após cada fase, além dos testes existentes de sobreposição com
BRK/IRQ. CTest headless: **48/48**. Resultado completo:
`build-headless/accuracycoin/nmi-sampling-final.txt`. O estado pendente da PPU
já era serializado e o formato não mudou; versões anteriores podem retomá-lo
em outra fase. PAL mantém a ordem anterior, sem validação adicional. As
verificações `NMI Suppression`, `NMI at VBlank end` e `NMI disabled at VBlank`
continuam falhando: este ajuste não modela completamente a linha de NMI nem
todos os alinhamentos físicos de CPU/PPU. Referência:
[NESdev: interrupções da CPU](https://www.nesdev.org/wiki/Interrupts).

### Cancelamento de NMI antes da amostragem

Em 2026-11-13, o AccuracyCoin avançou de **131 para 134/147**: `NMI
Suppression`, `NMI at VBlank end` e `NMI disabled at VBlank` passaram; as
outras 144 linhas mantiveram os mesmos resultados e códigos. Uma NMI ainda
pendente na PPU é descartada quando a leitura de `$2002`, a desativação de
NMI em `$2000` ou o fim do VBlank tornam a linha inativa antes de a CPU a
amostrar. Uma interrupção já entregue à CPU continua pendente nela.

O teste `cpu-interrupt` cobre as três formas de cancelamento, leitura sem
efeitos, reativação durante VBlank e entrega já efetuada. CTest headless:
**48/48**. Resultado: `build-headless/accuracycoin/nmi-cancel-experiment.txt`.
O formato de save state não mudou. Este ajuste não modela explicitamente o
nível anterior da linha na CPU, pelo que transições múltiplas entre duas
amostragens e a temporização PAL ainda não estão validadas.

### Investigação da temporização de `$2002`

O teste `$2002 flag timing` ainda falha com código 1, na primeira parte:
ele compara a leitura dos três flags em torno do fim do VBlank. Segundo os
comentários da própria ROM, o bit de VBlank é capturado no começo da leitura,
enquanto os bits de sprite são observados perto do fim, aproximadamente dois
dots de PPU depois. A implementação atual de `PPU::cpuRead` obtém os três
bits de `ppuStatus` na mesma chamada, antes dos três clocks de PPU do passo
em `NES::step`. Esta diferença é uma hipótese para a falha, não uma prova
de que seja a única causa; a segunda parte do teste também verifica quando
os flags de sprite são ativados.

Uma correção requer investigar a fase da leitura no barramento CPU/PPU,
incluindo leituras descartadas e DMA, e manter os efeitos de `$2002` na fase
correta. Antecipar a limpeza dos flags de sprite apenas para satisfazer a
primeira parte alteraria a temporização do próprio estado da PPU e não
representaria a diferença entre os instantes de amostragem. Nenhuma alteração
de comportamento foi aplicada nesta investigação.

Em 2026-11-14, uma instrumentação do executor registrou os quatro valores da
primeira etapa: **`E0 E0 E0 00`**, contra `E0 E0 80 00` ou `E0 80 80 00`
aceitos pela ROM. As leituras ocorreram, respectivamente, na scanline 260/dot
340 e no pré-render/dots 0, 1 e 2. Isso confirma a discrepância observável,
mas não prova por si só a causa elétrica.

Como experimento temporário, somente o valor retornado dos bits de sprite foi
limpo nas leituras iniciadas nos dots 0 e 1 do pré-render. A primeira etapa
passou, mas o teste continuou falhando com **código 2**, na etapa que verifica
a ativação dos flags: os resultados em `$51–$58`, após a máscara da ROM, foram
`00 00 00 00 00 60 60 00`, e a ROM exige `$55 = 60`. A experiência não foi
mantida: ela não modela a evolução da PPU durante a leitura nem resolve a
ativação dos flags. A pontuação permaneceu em **135/147**. Uma correção deve
tratar também as leituras reais e descartadas durante DMA, a amostragem de NMI
e a fase PAL, além de preservar os efeitos colaterais de `$2002`.

### Leitura de `$2002` em fases e overflow

Em 2026-11-14, o AccuracyCoin passou de **135 para 136/147**: `$2002 flag
timing` passou e os outros 146 resultados e códigos ficaram iguais. Em passos
NTSC, a leitura mantém o VBlank capturado no início e avança dois dots antes
de capturar os bits de sprite. Os efeitos de limpeza de VBlank e cancelamento
da borda pendente continuam no início; a amostragem de NMI do primeiro dot é
entregue à CPU depois do ciclo corrente. Isso também se aplica às leituras
descartadas que passam pelo barramento durante DMA. Leituras de inspeção não
avançam a PPU.

A segunda etapa também exigiu que o flag de overflow ficasse visível um dot
após a comparação da avaliação de sprites. `ppu-sprite-overflow` verifica a
transição, a restauração com overflow pendente e leituras reais da CPU nas três
fases em torno da limpeza do pré-render. O bit pendente é armazenado num bit
antes reservado do estado da PPU: estados antigos continuam legíveis, mas
versões anteriores rejeitam um estado salvo nessa janela de um dot.

O tratamento de leitura em fases ainda não foi validado para PAL; nessa região
a ordem anterior da leitura foi mantida. O atraso do overflow se aplica às duas
regiões, mas não foi comparado com hardware PAL. Em NTSC, os dois dots
antecipados ocorrem antes do tick do cartucho nesse ciclo; interações finas com
mappers e overclock durante a leitura ainda não foram validadas em hardware.
CTest headless: **48/48**. Resultado final:
`build-headless/accuracycoin/flag-phased-final.txt`.

### Endereço da OAM secundária durante a busca

Em 2026-11-14, `Misaligned OAM2 Address` passou e o AccuracyCoin avançou de
**136 para 137/147**; as outras 146 linhas mantiveram seus resultados e códigos.
A avaliação grava o Y candidato na OAM secundária mesmo quando ele está fora
de faixa, sem avançar o endereço. Durante a busca, esse endereço avança com os
quatro bytes de cada slot apenas enquanto a renderização está ativa. Depois da
busca, `$2004` continua lendo a OAM secundária pelo endereço alcançado.

O teste `ppu-sprite-overflow` cobre o último Y rejeitado, a interrupção da
busca e a restauração de um estado durante essa interrupção. O endereço salvo
passou a ter significado durante a busca; os formatos de save state agora são
`R2N0` (NTSC) e `R2N1` (PAL), e estados de versões anteriores são rejeitados.
O modelo ainda não usa os bytes desalinhados da OAM secundária para alimentar
todos os campos das unidades de sprite, nem implementa integralmente o
congelamento do incremento da OAM2. O teste da ROM, por si, não valida esses
casos. CTest headless: **48/48**. Resultado final:
`build-headless/accuracycoin/oam2-address-final.txt`.

### Contadores de sprite durante forced blanking

Em 2026-11-13, foi corrigida uma parte do comportamento de `Stale Sprite
Shift Regs`: durante os dots visíveis, os contadores X continuam a avançar
mesmo quando a renderização está desligada; os shifters aguardam a sua
reativação. O teste `ppu-sprite-overflow` verifica tanto o contador antes de
chegar a zero quanto a pausa dos pixels depois de começar a emitir. A versão
anterior falha no primeiro caso. Referência: [pesquisa sobre renderização de
sprites no NESdev](https://forums.nesdev.org/viewtopic.php?t=26291).

O AccuracyCoin permanece em **134/147** e as 147 linhas mantêm os mesmos
resultados e códigos (`build-headless/accuracycoin/sprite-counter-blanking.txt`).
O teste composto de sprites antigos continua falhando no código 3: a troca
das unidades entre linhas ainda não preserva os shifters durante períodos
prolongados com rendering desligado. A contagem fora dos dots visíveis também
não foi modelada nesta mudança. O formato de save state não mudou.

### Preservação de sprites após a interrupção da busca

Em 2026-11-13, o AccuracyCoin avançou de **134 para 135/147**: `Stale Sprite
Shift Regs` passou. Quando não há busca de padrões, as unidades de sprite da
linha anterior são preservadas; a carga do contador X foi movida para a fase
posterior da busca. O teste `ppu-sprite-overflow`, com o limite normal de oito
sprites, cobre várias linhas em branco, interrupção no início da busca e
save/load. Resultado: `build-headless/accuracycoin/sprite-stale-phase2.txt`.

A troca de unidades ainda é feita por linha: buscas parcialmente executadas
em diferentes slots não são modeladas integralmente, e o teste da ROM não
valida todas essas combinações. O novo estado pendente é serializado e os save
states passaram a `R2NY` (NTSC) e `R2NZ` (PAL). Formatos anteriores são
rejeitados, pois não contêm esse estado; versões anteriores também não leem
o novo formato.

## Fork anterior de 125 testes

Execução em 2026-11-04 usando o núcleo R2NES, sem interface, com overclock
desativado. A ROM [AccuracyCoin](https://github.com/to11mtm/nes-AccuracyCoin)
foi baixada do repositório e executada com Start simulado no menu inicial.
SHA-256 da ROM: `95d1d731aacaca6cbdbd34c5183a0efee269fd728c21ee8e5d4d705a71c531ad`.
Os testes miram as revisões RP2A03G e RP2C02G, segundo o autor.

Dos 125 testes da execução automática, **123 retornaram**, com **87 PASS** e
**36 FAIL**. O teste 124, *Implied Dummy Reads*, não terminou; *JSR Edge Cases*,
o teste 125, não foi alcançado. Não há pontuação final da própria ROM: os
resultados parciais foram lidos das posições de RAM especificadas no código da
suíte. Os testes informativos DRAW não entram nessa contagem. PASS significa
apenas que as condições específicas daquele teste foram atendidas.

O processo continuou por 600 milhões de ciclos do sistema; a contagem ficou
em 123 desde antes dos 70 milhões e o PC repetiu o laço em `$921D–$9224`.
O código-fonte identifica esse trecho como `DMASync48_Loop` e avisa que ele
pode não terminar se o comportamento de open bus necessário não estiver
emulado. Isso identifica o ponto do bloqueio, não prova isoladamente qual
diferença de CPU, DMA ou barramento o causou.

O teste `$BB LAE absolute,Y` (LAS) retornou PASS. Os resultados de CPU incluem
falhas de Open Bus (5), Unofficial Instructions (A), SHA indireto e absoluto
(F), SHS/TAS (F), SHY (1), SHX (1), Interrupt flag latency (8), NMI Overlap
BRK (2), NMI Overlap IRQ (1) e Instruction Timing (2).

Demais resultados FAIL, com código informado pela ROM:

- DMA + Open Bus (2), DMA + $2007 Read (2), DMA + $2007 Write (1), DMA +
  $4015 Read (2), DMA + $4016 Read (1), APU Register Activation (1), DMC DMA
  Bus Conflicts (1), DMC DMA + OAM DMA (1), Explicit DMA Abort (1), Implicit
  DMA Abort (1).
- Length Counter (9), Length Table (0), Frame Counter IRQ (7), Frame Counter
  4-step (5), Delta Modulation Channel (13 hexadecimal), Controller Strobing
  (4).
- VBlank beginning (1), NMI Timing (1), NMI Suppression (1), NMI at VBlank
  end (1), NMI disabled at VBlank (1), Misaligned OAM behavior (4), Address
  $2004 behavior (3), OAM Corruption (2), RMW $2007 Extra Write (1).

Os códigos são os da ROM, em hexadecimal; o README do projeto descreve as
condições de cada código. Uma falha pode refletir uma diferença em outro
componente usado como pré-requisito, e não necessariamente um defeito isolado
no componente nomeado.

O executor temporário, a ROM, o código-fonte da suíte e o dump da execução de
150 milhões de ciclos estão em `build-headless/accuracycoin/`. A execução de
150 milhões reproduziu o mesmo bloqueio e os mesmos resultados parciais.
