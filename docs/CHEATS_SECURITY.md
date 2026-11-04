# Cheats: análise de segurança e proposta

Registro para orientar um trabalho futuro no R2NES. A proposta abaixo ainda não
foi implementada. A intenção é que o projeto disponibilize a coleção de cheats;
o usuário poderá ativar ou desativar as opções oferecidas, sem adicionar novos
cheats pela aplicação.

## Situação atual

- A coleção usa arquivos `.cht` em `cheats/`, identificados pelo CRC32 da ROM.
  O `NES` seleciona o arquivo ao carregar um cartucho.
- `CheatManager` lê o arquivo, interpreta descrições e códigos, e mantém um
  interruptor geral, inicialmente desligado, além dos estados individuais.
- Game Genie altera o valor devolvido em leituras do barramento; códigos de RAM
  podem congelar bytes da RAM interna ou solicitar escritas na faixa de memória
  do cartucho. Códigos compostos podem conter várias operações.
- O decodificador já limita as faixas aceitas para códigos de RAM e rejeita
  códigos inválidos. Os testes existentes cobrem esses comportamentos e alguns
  exemplos da coleção.

## Modelo de risco

Um cheat atua sobre o estado ou as leituras **emuladas** do NES; isso não é,
por si só, uma escrita arbitrária na memória do computador. Ainda assim, um
cheat incorreto pode travar ou alterar o comportamento de um jogo e modificar
memória de cartucho, inclusive dados posteriormente persistidos em bateria.
Falhas no parser, no barramento, no mapper ou no restante do núcleo podem ter
consequências mais graves. Uma coleção compilada não torna o núcleo imune a
entradas inesperadas, e ainda é necessário testá-lo e validar as operações.

Pontos concretos identificados na leitura inicial:

1. O parser de `.cht` em `src/Core/Cheats/CheatManager.cpp` lê linhas e acumula
   entradas sem limites explícitos de tamanho ou quantidade. Um arquivo local
   adulterado pode provocar consumo excessivo de recursos.
2. `CheatManager::applyRam` encaminha escritas de `$6000` a `$7FFF` para
   `Cartridge::cpuWrite`. A interpretação dessa faixa depende da placa; não se
   deve assumir que uma escrita genérica nela sempre modifica apenas RAM.
3. CRC32 serve para identificar ROMs, mas não oferece resistência adequada a
   colisões intencionais nem autentica a origem dos dados.

Esses pontos resultam de análise de código; não foi demonstrada exploração ou
execução de código. A revisão completa dos cheats ainda está pendente.

## Proposta: fontes `.cht`, catálogo compilado

Manter os arquivos `.cht` legíveis e versionados como fonte de manutenção da
coleção, mas convertê-los **durante o build** em uma tabela de dados tipados
compilada no executável:

```text
cheats/*.cht -> gerador e validador -> tabela gerada -> executável e menu
```

O gerador deve validar entradas, limites, códigos compostos, identificadores e
duplicações; produzir saída determinística; e tratar corretamente descrições e
caracteres especiais ao gerar código ou recursos. Uma entrada inválida deve
produzir erro claro no build. O catálogo gerado é um artefato, não uma coleção
a ser editada manualmente em C++ ou em um binário.

Na execução, a aplicação consultaria somente o catálogo embutido, deixando de
carregar `.cht` arbitrários do disco. Ativar e desativar cheats continuaria
disponível. A coleção passaria a ser atualizada junto com o aplicativo. Embutir
dados reduz essa superfície de entrada, mas não impede alguém com controle da
máquina de alterar ou substituir o próprio executável.

As operações devem ser explícitas: substituição de leitura Game Genie, escrita
na RAM interna e, se necessária, operação específica para RAM do cartucho. Para
esta última, avaliar uma API na qual o mapper autorize e realize acesso à RAM
adequada, sem usar uma escrita genérica de registrador. Um mapper que não
ofereça essa capacidade deverá recusar a operação. Verificar também o efeito
sobre memória volátil e persistente de cada placa.

## Identidade da ROM

Avaliar um identificador resistente a colisões, como SHA-256, definido sobre
uma representação precisa dos dados da ROM **e dos metadados de placa que
afetam a interpretação dos cheats**. O mesmo conteúdo pode aparecer em
contêineres diferentes ou com cabeçalhos diferentes: a regra de normalização
precisa ser documentada e testada antes da migração.

Hash forte auxilia a selecionar o catálogo correto, mas não autentica uma ROM
nem garante que um cheat seja seguro. Os `.cht` atuais são nomeados por CRC32;
será necessário associá-los às imagens e configurações correspondentes para
migrar a coleção. Não supor que o CRC32 antigo, isoladamente, permita obter o
SHA-256 ou desambiguar todas as ROMs.

## Alternativas

- **Manter `.cht` externos:** fáceis de inspecionar e atualizar separadamente,
  mas exigem parser e limites em tempo de execução e podem ser alterados no
  disco. Adequado caso a atualização independente se torne um requisito.
- **SQLite:** útil para uma coleção grande, consultas complexas ou atualização
  independente, mas acrescenta uma dependência e um formato a tratar. Um banco
  externo também pode ser alterado; veja as recomendações oficiais do
  [SQLite para dados não confiáveis](https://www.sqlite.org/security.html).
- **Binário próprio externo:** reúne os dados, mas continua exigindo um leitor
  robusto, limites e uma política de atualização. Não traz automaticamente as
  vantagens de eliminar a leitura de arquivos externos.

## Sequência sugerida

1. Inventariar os `.cht` existentes, seus tipos de códigos, ROMs e mappers;
   confirmar quais operações de RAM do cartucho são realmente necessárias.
2. Implementar o gerador e testes para entradas válidas, inválidas, duplicadas e
   limites, verificando equivalência com a coleção atualmente interpretada.
3. Definir e testar a identidade das ROMs e a associação dos arquivos atuais.
4. Projetar e testar o acesso explícito à RAM do cartucho por placa, incluindo
   proteção de registradores e efeitos sobre dados de bateria.
5. Integrar o catálogo compilado, preservar o menu e os controles existentes,
   e só então remover o carregador de `.cht` em tempo de execução.
6. Executar a suíte e testes de segurança com sanitizers; documentar as
   limitações conhecidas e os resultados.
