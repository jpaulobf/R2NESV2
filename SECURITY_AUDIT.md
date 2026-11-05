# Revisão de segurança: entradas do emulador

Revisão de 2026-11-04, focada em arquivos e dados que chegam ao núcleo e à
interface. Não é uma auditoria exaustiva nem demonstração de exploração. Os
cheats foram explicitamente deixados para uma rodada separada.

## Problemas encontrados e corrigidos

### Nome de ROM interpretado como formato

O nome do arquivo era concatenado à string usada como formato de `snprintf`
no título da janela. Um nome com especificadores como `%n` era interpretado
como instrução de formatação sem os argumentos correspondentes, causando
comportamento indefinido e potencial corrupção de memória. Uma exploração ou
execução de código não foi demonstrada.

O título agora usa um formato constante para FPS e concatena o nome como
dados. O teste de integração SDL usa nomes com `%n`, `%s` e `%08x`,
inclusive quando se troca a ROM sem alterar o FPS.

### Contagem de sprites inconsistente após carregar estado

A validação anterior limitava a contagem de sprites e o índice de avaliação
separadamente, mas um save adulterado podia combinar contagem 64 e índice de
avaliação menor. Com Unlimited Sprites habilitado, a PPU podia tentar gravar
uma entrada além do array de 64 sprites. Uma exploração não foi demonstrada.

A avaliação agora verifica a capacidade antes de gravar ou incrementar, e a
renderização limita sua iteração ao array disponível. Testes constroem estados
com contagem 64 em ambos os caminhos de avaliação. Essas proteções também
tratam estados intermediários inconsistentes sem depender de um modo fixo de
Unlimited Sprites durante toda a linha.

## Verificação e limites

- `sdl3-integration` verifica o título com nomes adversos.
- `state-security` verifica os limites da avaliação de sprites.
- `hostile-state` aplica 1.200 mutações determinísticas a campos de CPU, PPU,
  APU e MMC3, exercita estados aceitos com renderização e sprites ilimitados e
  confere rollback dos rejeitados. Não cobre todos os bytes nem outros mappers.
- O leitor ZIP, ROMs e estados têm testes específicos adicionais. ASan e UBSan
  ajudam a identificar falhas executadas pelos testes; não provam ausência de
  vulnerabilidades nem validam a possibilidade de execução de código.

Ainda são recomendáveis fuzzing orientado por cobertura e de longa duração,
auditoria das variantes de mapper e dos caminhos de interface/plataforma,
validação semântica mais completa dos save states e limites para o arquivo de
configuração. Cheats permanecem fora do escopo desta revisão.
