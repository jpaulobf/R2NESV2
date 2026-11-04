# Auditoria inicial dos estados de CPU, PPU e APU

Esta revisão acompanha o carregamento de save states de CPU, PPU, APU e mappers.
O protocolo completo de DMA e os demais formatos de entrada não foram auditados
em profundidade. `R2NK` continua sendo o formato gravado; estados com valores
inválidos passam a ser rejeitados.

## CPU

- `opcode` é um byte usado para indexar a tabela de 256 instruções; os
  registradores e endereços têm largura fixa. Seus valores isolados não são
  índices de memória do host.
- `cycles` e `microCycle` controlam o avanço da instrução. O carregador limita
  ambos a oito, o máximo das instruções atuais; esperas de DMA pertencem ao
  escalonador do console. A combinação exata de opcode, ciclo e flags ainda não
  é validada como uma máquina de estados completa.
- As flags serializadas são lidas como bytes 0 ou 1, e `interruptKind` aceita
  apenas 0 a 3. Estados truncados ou flags inválidas sinalizam erro no stream.

## PPU

- Os limites existentes de `scanline`, `cycle`, `fineX`, contagem de sprites,
  avaliação de OAM, endereço secundário e leitura pendente protegem os caminhos
  que indexam framebuffer/OAM/sprites ou aplicam deslocamentos. `oamIndex` de
  cada sprite é verificado antes do uso; leituras de paleta são mascaradas.
- Endereços, registradores, bytes de pipeline, OAM, paleta e framebuffer são
  dados de largura fixa. Nem toda combinação semanticamente impossível é
  rejeitada: por exemplo, ainda não há uma validação relacional completa entre
  os sprites, o dot e o estágio de avaliação.
- A leitura atual aplica campos antes de concluir a validação. A fachada `NES`
  tenta restaurar um snapshot anterior se a leitura falhar; um carregamento
  direto de componente com erro não é transacional.

## APU

- Modos e posições de duty e deslocamentos de sweep são validados antes que
  possam indexar tabelas ou ser usados como operandos de deslocamento.
- Modos e contador do sequenciador, atraso de escrita, envelopes, sweep,
  contadores lineares, estado do DMC e acumulador de amostras recebem limites
  derivados dos registradores e do ciclo do núcleo. A validação do acumulador
  admite estados capturados em qualquer taxa de áudio aceita, mesmo que a taxa
  atual do host seja diferente.
- Valores de ponto flutuante serializados usados pelo acumulador e pelos filtros
  devem ser finitos. Não foi provada estabilidade numérica para todo valor
  finito artificialmente escolhido.
- Timers e demais bytes são dados de largura fixa; nem toda relação temporal
  entre canais, timers, DMA e sequenciador está validada.

## Verificação e limites

`state-security` altera campos representativos, verifica que os componentes ou
o console sinalizam erro e exercita rollback do console. Também carrega estados
de APU capturados durante a execução, inclusive após mudanças de modo e com
taxas de áudio diferentes. A suíte de CPU percorre opcodes e estados
intermediários; os testes existentes de PPU e mappers complementam a amostra.

Essa cobertura não constitui uma prova de segurança. Permanecem a validação
relacional abrangente, a leitura transacional dos componentes, casos em todos
os pontos de DMA/renderização, cobertura adicional dos mappers e fuzzing sustentado sob
sanitizers. Cheats ficam fora deste trabalho.

## Mappers

Revisão focada nos leitores de estado, não uma auditoria completa da precisão
ou de todas as combinações possíveis de cada placa:

- Mapper 000 não serializa registradores. Os mappers 1, 2, 3, 4, 5, 7, 9, 11 e
  150 já usam leitura em temporários ou cópia com validação; 79 e 147 herdam o
  leitor de 11. Os testes específicos existentes cobrem casos variados de
  truncamento, limites e rollback.
- Os leitores de 23, 40 e 90 agora validam booleanos antes de atribuí-los,
  rejeitam valores inconsistentes selecionados e só aplicam o estado completo.
  Em 23, o prescaler e a seleção de bancos também são limitados; em 90 os
  bancos CHR são conferidos contra seus registradores.
- Os leitores de 66 e 71 validam os bancos antes de aplicá-los; 71 também
  confere o espelhamento permitido para sua configuração.
- Os derivados MMC3 187 e 189 agora carregam base e registrador adicional em
  uma cópia, evitando aplicar a base quando o último byte está ausente. O
  seletor de PRG do 189 também é limitado ao tamanho do registrador.
- `mapper-state-security` verifica truncamentos, flags, bancos, espelhamento,
  prescaler e rollback direto dos mappers alterados, além de rollback do console
  para um estado corrompido do mapper 23. Não cobre toda a máquina de estados,
  todas as variantes de placa ou todos os padrões de entrada malformada.

O formato não foi alterado; a validação mais estrita pode rejeitar estados
antigos com valores que o novo leitor considera inválidos. Fuzzing abrangente,
verificação dos acessos reais durante emulação e a revisão do rollback de todo
o console continuam pendentes.
