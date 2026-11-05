# Revisão inicial da CPU 2A03

Revisão de 2026-11-04. Esta é uma primeira análise, não uma validação
exaustiva de instruções, ciclos, interrupções ou interação com APU e PPU.
O manual original da MOS documenta o 6502, mas não cobre as diferenças do
2A03 nem os opcodes não oficiais. Para estes pontos, as referências abaixo
incluem pesquisa independente sobre hardware e simulação, não documentação
oficial da Nintendo ou Ricoh.

## Corrigido

- NMI durante BRK ou IRQ pode assumir a seleção do vetor até o ciclo 5,
  preservando o endereço de retorno e o bit B empilhado. A escolha do vetor
  sobrevive ao save state. Testes sintéticos cobrem bordas após os ciclos 1–6,
  os bits empilhados e NMI tardia; não são uma validação completa de timing.
  Os testes sintéticos também verificam latência de IRQ de CLI, SEI, PLP e RTI.
  Save states usam `R2NL`; formatos anteriores, incluindo `R2NK`, são rejeitados.
- As portas de controle `$4016/$4017` preservam os três bits superiores do
  barramento aberto; leituras de `$4015` preservam o barramento externo e
  retornam o bit 5 aberto. A ROM AccuracyCoin de 147 testes passou de 93 para
  94 na execução headless: Open Bus passou; não houve regressão PASS/FAIL.
- `LAS $BB`: o resultado de memória AND SP era copiado para A e SP, mas não
  para X. Agora os três recebem o resultado. O teste exercita N, Z e leitura
  com cruzamento de página. É um opcode não oficial; a tabela de resultados
  do Visual6502 indica A, X e SP como destinos.

## Diferenças e pontos pendentes

- `NES::reset()` usa uma inicialização determinística de sistema, inclusive
  limpando a RAM, e `CPU::reset()` zera A, X e Y e reinicializa SP e status.
  Isso não representa o reset elétrico durante uma sessão: medições do 2A03
  indicam A/X/Y e a RAM preservados, SP decrementado em três e I ligado.
  Corrigir exige distinguir inicialização de reset em todo o console, incluindo
  APU, PPU e mappers; não basta alterar isoladamente a CPU.
- A seleção tardia do vetor foi implementada, mas o timing completo das
  interrupções ainda difere do hardware: AccuracyCoin continua falhando em
  NMI Overlap BRK (2), NMI Overlap IRQ (1) e Interrupt Flag Latency (A).
  Os diagnósticos Blargg `2-nmi_and_brk`, `3-nmi_and_irq` e
  `5-branch_delays_irq` também falharam nesta rodada; `1-cli_latency` passou.
  As causas exatas e o papel da sincronização PPU/DMC ainda precisam ser
  isolados. O polling de branches anterior foi preservado.
- `NES::step()` informa explicitamente que a arbitragem entre DMA de OAM e
  DMC não é modelada ciclo a ciclo. Isso pode afetar timing e interações de
  DMA; não implica por si só que o áudio ou sprites falhem em todos os jogos.
- O teste Internal Data Bus falha no pré-requisito que envolve DMC DMA. A
  distinção entre barramentos interno e externo durante DMA ainda não está
  modelada completamente; o bit 5 de `$4015` usa o latch externo existente.
- Os diagnósticos históricos `4-jitter` e `6-irq_flag_timing` falharam em
  revisão anterior e não foram reexecutados nesta rodada. Suas causas não
  estão todas demonstradas.
- `XAA` usa uma aproximação determinística, e as escritas não oficiais SHA,
  SHX, SHY e TAS merecem verificação específica, inclusive cruzamento de
  página e DMA. Não inferir comportamento universal de opcodes instáveis.
- O teste de hash dos 256 opcodes verifica regressões contra comportamento
  anterior do emulador, não contra hardware. Seu hash foi atualizado após a
  correções intencionais de LAS e interrupções; testes independentes são
  necessários para estabelecer fidelidade.

## Referências

- [MOS Technology, MCS6500 Programming Manual, 1976](https://www.bitsavers.org/components/mosTechnology/6500-50A_MCS6500pgmManJan76.pdf).
- [Visual6502: resultados dos 256 opcodes](https://www.nesdev.org/wiki/Visual6502wiki/6502_all_256_Opcodes).
- [NESdev: estado de inicialização e reset da CPU](https://www.nesdev.org/wiki/CPU_power_up_state).
- [NESdev: interrupções](https://www.nesdev.org/wiki/Interrupts).
- [NESdev: opcodes não oficiais](https://www.nesdev.org/wiki/CPU_unofficial_opcodes).
