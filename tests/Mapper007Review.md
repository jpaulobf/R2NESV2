# Revisão do Mapper007 (AxROM)

Revisão da implementação e do carregamento de cartuchos, comparando seu
comportamento com a documentação do NesDev. As alterações foram registradas
em `Unreleased`; não fazem parte da tag `v1.0.5` já criada.

## Comportamento documentado

AxROM seleciona uma janela PRG de 32 KiB em `$8000–$FFFF`. Os bits 0–2 do
registrador selecionam o banco e o bit 4 escolhe uma das duas páginas de 1 KiB
da CIRAM, apresentada nas quatro nametables. A memória de padrões é CHR-RAM
fixa de 8 KiB. Não há PRG-RAM, IRQ ou áudio adicional na placa padrão.

O limite padrão é 256 KiB de PRG. A extensão de 512 KiB descrita pelo NesDev
utiliza também o bit 3, preservando o bit 4 para a CIRAM. Essa extensão já
existia no emulador e foi mantida.

No NES 2.0, o submapper 1 não tem conflitos de barramento; o 2 aplica AND
entre o valor escrito e o byte da ROM no endereço e banco anteriores à
escrita. Para iNES e submapper 0, a recomendação é emular sem conflitos,
preservando a compatibilidade com as placas que dependem de sua ausência.

## Discrepâncias encontradas e correções

| Área | Situação anterior | Resultado da revisão |
| --- | --- | --- |
| Configuração de PRG | Aceitava tamanhos arbitrários; módulo e máscara de até quatro bits produziam bancos incoerentes ou inacessíveis. | Aceita potências de dois até 512 KiB; linhas físicas ausentes espelham a memória. |
| CHR-ROM | O carregador aceitava CHR-ROM, mas o mapper permitia escrever nela como se fosse RAM. | Rejeita CHR-ROM; a placa emulada tem apenas CHR-RAM de 8 KiB. |
| Submappers | Valores desconhecidos caíam silenciosamente no comportamento sem conflitos. | NES 2.0 aceita apenas 0, 1 e 2. |
| RAM e flags | RAM/NVRAM e flags incompatíveis eram aceitas sem a memória correspondente. | Rejeita declarações incompatíveis, bateria, trainer, four-screen e Misc. ROMs declaradas no NES 2.0. |
| PRG truncada | Em iNES, o carregador podia reduzir o número de bancos para o conteúdo disponível. | Rejeita a imagem truncada antes de construir o mapper. |
| Leitura de estados | Alterava banco e enum diretamente, sem validar conteúdo ou conclusão da leitura. | Valida em variáveis temporárias; exige banco físico válido e uma das duas páginas single-screen antes de aplicar. |
| Formato de estados | Dependia do tamanho nativo do enum e da ordem de bytes da plataforma. | Grava explicitamente um byte de banco e quatro bytes de espelhamento em little-endian, compatíveis com o formato anterior no Windows. |
| Testes | Cobriam um tamanho de PRG e exemplos de conflitos, sem integração ao CTest. | Cobrem capacidades, valores do registrador, memória, estados, carregamento e integração com o console; disponíveis pelo CTest. |

A troca de bancos de 32 KiB, o controle single-screen pelo bit 4 e a política
de conflitos em `Cartridge::cpuWrite` já estavam corretos para configurações
válidas. Foram preservados e receberam verificações adicionais. Os conflitos
continuam no cartucho, que tem acesso ao byte real da PRG-ROM.

## Compatibilidade e limites deliberados

- PRG de 16 KiB continua aceita como extensão de compatibilidade, espelhada
  nas duas metades da janela de 32 KiB. Isso não identifica uma placa AxROM
  comercial documentada.
- O bit H/V do cabeçalho é ignorado: o registrador controla a nametable.
  Four-screen é rejeitado porque exigiria outra configuração de hardware.
- O byte 8 de cabeçalhos iNES antigos permanece ignorado, incluindo valores
  escritos por ferramentas antigas. Ele não seleciona variantes nem cria RAM.
  Cabeçalhos NES 2.0 precisam declarar a memória efetivamente emulada.
- Estados antigos válidos conservam o formato de cinco bytes. Estados de
  configurações agora rejeitadas não tornam essas configurações suportadas;
  estados com bancos ou espelhamento inválidos também são recusados.
- `reset()` mantém a inicialização completa e determinística usada pelo
  emulador: banco zero e CIRAM inferior, sem apagar CHR-RAM. O reset elétrico
  do console e a indeterminação do latch ao ligar exigem tratamento distinto
  no sistema; não foram simulados por mudanças isoladas neste mapper.
- Esta revisão não demonstra compatibilidade completa de jogos, precisão
  geral da CPU/PPU ou a resolução dos problemas de timing de *Battletoads*.

## Validação

A suíte `mapper007` usa imagens sintéticas, sem exigir ROMs comerciais:

- Todos os 256 valores do registrador nos limites da janela PRG, para
  capacidades de 16, 32, 64, 128, 256 e 512 KiB.
- Bits ignorados, independência entre banco PRG e página de nametable,
  tamanhos NES 2.0 lineares e por expoente/multiplicador.
- iNES e submappers 0/1/2, inclusive conflitos sequenciais com bytes distintos
  no mesmo endereço em bancos diferentes e proteção da PRG-ROM contra escrita.
- Leitura e escrita dos 8 KiB de CHR-RAM, acessos de sprites e ausência de
  PRG-RAM, com barramento aberto no console.
- As quatro nametables apontando para a mesma página, troca entre as duas
  páginas físicas e preservação de seus conteúdos.
- Estados antigos válidos, bancos e modos inválidos, todas as truncagens
  do payload do mapper e rollback do console após falhas de carregamento.
- Rejeição de imagens truncadas e configurações de memória não suportadas,
  tanto no carregador quanto no construtor direto do mapper.

Os comandos para executar a suíte estão em [README.md](README.md#mapper-7-regression-tests).

Na validação desta revisão, a compilação do emulador e dos cinco executáveis
de teste foi concluída. Os 12 testes registrados no CTest passaram, incluindo
a nova suíte do Mapper007, as suítes dos Mappers001 a 004 e as sete ROMs de
IRQ do MMC3 disponíveis no projeto. `git diff --check` não apontou erros.

## Referências

- [NesDev — AxROM](https://www.nesdev.org/wiki/AxROM)
- [NesDev — NES 2.0 submappers](https://www.nesdev.org/wiki/NES_2.0_submappers)
- [NesDev — NES 2.0](https://www.nesdev.org/wiki/NES_2.0)
- [NesDev — iNES](https://www.nesdev.org/wiki/INES)

A implementação foi elaborada a partir da especificação e do código deste
projeto; esta revisão não usou código-fonte de outros emuladores como modelo.
