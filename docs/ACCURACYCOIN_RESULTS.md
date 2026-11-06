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
