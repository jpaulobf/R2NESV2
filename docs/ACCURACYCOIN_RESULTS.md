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
