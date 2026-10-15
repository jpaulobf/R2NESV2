# Revisão do Mapper009 (MMC2 / PxROM)

O foco desta revisão é proteger o comportamento de Punch-Out!! que já funciona
e corrigir discrepâncias de hardware, carregamento, depuração e persistência.
As alterações pertencem a `Unreleased`, após a tag `v1.0.5`.

## Resultado da análise

| Área | Constatação | Tratamento |
| --- | --- | --- |
| PRG | A janela selecionável de 8 KiB e os três bancos finais fixos já estavam corretos para o cartucho padrão. Configurações pequenas ou grandes demais eram aceitas. | Mantido o arranjo; validação de capacidades e espelhamento por linhas físicas. |
| Latches de CHR | A leitura disparadora já usava o banco anterior; os endereços inferiores exatos distinguiam MMC2 de MMC4 corretamente. | Preservado e coberto por testes para todos os endereços de CHR nos dois estados de latch. |
| RAM | Existiam 8 KiB de RAM acessíveis em `$6000–$7FFF`; PxROM não tem essa memória. | Removido o mapeamento; o barramento da CPU conserva seu último valor. |
| Escritas de registradores | Alteravam o mapper, mas retornavam `false`, indicando acesso não atendido. | Retornam `true` e o sentinela de registrador interno em `$A000–$FFFF`. |
| CHR-RAM | Havia um fallback com bancos que podiam ultrapassar os 8 KiB alocados. | Rejeitada a configuração: esta implementação representa a placa com CHR-ROM. |
| Pattern Table Viewer | As consultas percorriam endereços disparadores e alteravam os latches do jogo. | Adicionado caminho de consulta sem efeitos para MMC2, usado pelo visualizador. |
| Cabeçalhos | Tamanhos e variantes incompatíveis eram aceitos; imagens iNES truncadas podiam ter sua capacidade reduzida silenciosamente. | Validação antes da construção, incluindo rejeição de PRG/CHR truncadas. |
| Save states | Leitura direta nos campos permitia aplicar estados incompletos ou valores inválidos. | Validação completa em temporários antes de alterar o mapper; enum gravado explicitamente em little-endian. |

O MMC2 não tem IRQ nem conflitos de barramento. O código não acrescenta esses
recursos. As escritas em `$8000–$9FFF` não selecionam registradores e não alteram
a PRG-ROM. O bit 0 de `$F000–$FFFF` seleciona vertical (0) ou horizontal (1).

## Latches e integração com a PPU

Os quatro registradores CHR retêm cinco bits e representam bancos de 4 KiB.
Cada metade do espaço de padrões possui um latch independente:

| Leitura | Efeito nas leituras seguintes |
| --- | --- |
| `$0FD8` | Metade inferior usa o registrador de `$B000`. |
| `$0FE8` | Metade inferior usa o registrador de `$C000`. |
| `$1FD8–$1FDF` | Metade superior usa o registrador de `$D000`. |
| `$1FE8–$1FEF` | Metade superior usa o registrador de `$E000`. |

A tradução do endereço acontece antes da atualização do latch. Escritas em
CHR-ROM e consultas do visualizador não disparam mudanças. Leituras reais
via PPUDATA e buscas de sprites continuam acionando o mecanismo; uma leitura
de depuração de PPUDATA não o aciona.

O caminho `ppuMapPeek` tem implementação específica no MMC2. Os demais mappers
mantêm o comportamento anterior por padrão; isso não representa uma garantia
nova de ausência de efeitos de depuração para todos os mappers existentes.

## Configurações e estados

- PRG: 32, 64 ou 128 KiB. O mínimo permite representar os três bancos fixos
  de 8 KiB sem subtrações inválidas; o máximo corresponde aos quatro bits PRG.
- CHR-ROM: 8, 16, 32, 64 ou 128 KiB, dentro dos cinco bits CHR. Capacidades
  menores espelham os bancos conforme as linhas físicas disponíveis.
- NES 2.0: apenas submapper 0, sem RAM/NVRAM PRG ou CHR e sem Misc. ROMs.
  CHR-RAM, bateria, trainers e four-screen não fazem parte da placa emulada.
  O byte 8 ambíguo de iNES antigos não cria memória ou variantes.
- Estados conservam os 8207 bytes do formato Windows anterior: cinco bytes
  de registradores, dois latches, dois modos de espelhamento de 32 bits e
  8192 bytes reservados. A antiga RAM fictícia é consumida, mas ignorada.
  Isso mantém o alinhamento dos componentes sem perpetuar a RAM no hardware.
- Registradores mantêm seus bits mesmo quando não há todas as linhas físicas
  na ROM. Um valor válido no registrador não precisa ser um banco físico único.
- O modo de espelhamento atual precisa ser H/V; o modo inicial salvo precisa
  corresponder ao cabeçalho do cartucho carregado. Todos os latches e campos
  são validados antes da aplicação; falhas no estado do console fazem rollback.

## Limites e validação

`reset()` mantém a convenção anterior do emulador: registradores zerados,
latches FE e espelhamento inicial do cabeçalho. Isso é inicialização completa,
não uma afirmação de estado elétrico garantido do MMC2.

A suíte sintética exercita bancos, máscaras, latches, proteção da ROM,
barramento aberto, visualizador, PPUDATA, espelhamento pela PPU e estados.
Todas as 8207 possibilidades de truncagem do payload são verificadas.
Os comandos estão em [README.md](README.md#mapper-9-regression-tests).

Validação desta revisão: emulador e executáveis de teste compilados; os 13
testes registrados no CTest passaram, incluindo Mapper009, Mappers001 a 004,
Mapper007 e as sete ROMs de IRQ do MMC3 disponíveis no projeto.
`git diff --check` não apontou erros de whitespace.

O modo Unlimited Sprites continua com seu caminho de composição expandida,
que pode fazer leituras adicionais de padrões. A revisão geral desse modo
permanece pendente; esta alteração não promete equivalência ao hardware com
essa opção habilitada. Partidas completas de Punch-Out!! ficam para o teste
manual, sem alteração de sua classificação na lista de compatibilidade.

## Referências e limite da consulta

- [NesDev — MMC2](https://www.nesdev.org/wiki/MMC2): referência principal do mapper;
  a consulta direta nesta sessão foi bloqueada pelo site (HTTP 403).
- [NesDev — MMC4](https://www.nesdev.org/wiki/MMC4): comparação explícita com MMC2,
  incluindo a ausência de PRG-RAM no MMC2 e as diferenças no chaveamento.
- [NesDev — discussão dos latches de MMC2](https://forums.nesdev.org/viewtopic.php?t=327):
  relação entre os quatro registradores CHR e os dois latches.
- [NesDev — discussão das conexões do MMC2](https://forums.nesdev.org/viewtopic.php?t=9325):
  disparo após a leitura do segundo plano dos padrões.

Os detalhes já existentes dos endereços de disparo foram preservados e
transformados em regressões; os testes verificam o contrato implementado,
sem substituir medições de uma placa real.
