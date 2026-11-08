# Home e miniaturas de save states

Estas decisões servem de referência para o R2NES e para a futura adaptação ao R2SEGA8 e ao R2BOY. A home descrita abaixo ainda não foi implementada.

## Miniaturas já implementadas no R2NES

- `src/System/GameStateManager.cpp` gera um PNG após salvar o estado com sucesso, a partir do framebuffer da PPU.
- Os arquivos ficam em `bin/savestate-thumbs`, com o nome da ROM e o número do slot: `<rom>.<slot>.png`.
- Antes de gerar a miniatura, são removidos 8 pixels de cada borda do quadro de 256 × 240. A área restante é redimensionada para 128 × 120.
- Há um arquivo por ROM e slot. Se a gravação falhar, a implementação tenta remover a miniatura antiga para não associá-la ao novo estado.
- A geração usa zlib; as dependências de compilação estão em `CMakeLists.txt`.

## Home planejada

- Ao abrir uma ROM, apresentar informações do jogo e três slots com miniaturas e opções de iniciar ou continuar.
- Durante o jogo, Esc abre a home e pausa a emulação; a interface oferece uma opção para voltar ao jogo.
- Cada slot oferece ações de salvar e carregar quando apropriado. Slots vazios devem ser identificados claramente.
- Inicialmente a home ocupa a janela existente. Seu layout deve se adaptar ao espaço disponível, permitindo uma futura apresentação em tela cheia.
- Separar a interface e suas ações do núcleo e da renderização específica de cada emulador para facilitar a adaptação aos demais projetos.

## Referência visual: RetroAssembly

As telas apresentadas pelo usuário são uma referência de fluxo e composição, para adaptação com identidade visual própria da família R2:

- **Apresentação ao abrir a ROM:** uma página com destaque para o jogo, suas informações e a ação de iniciar. A referência usa barras vermelhas, capa, imagens e metadados. No R2, podemos começar pelo nome da ROM e pelos estados disponíveis; capas e dados como gênero ou desenvolvedor dependem de uma fonte de dados e podem vir depois.
- **Pausa durante o jogo:** ao pressionar Esc, manter o quadro pausado visível sob uma camada escura translúcida. Exibir as ações principais e os estados salvos com suas miniaturas por cima, preservando o contexto do jogo e a legibilidade.
- Na pausa, considerar ações como Retomar, Reiniciar e Sair, além dos três slots de save state. O layout deve continuar utilizável na janela atual e se adaptar a uma futura apresentação em tela cheia.

## Segurança da leitura

Os PNGs ainda não são lidos pelo R2NES. Ao implementar a exibição, tratá-los como entrada não confiável: limitar tamanho do arquivo e dimensões antes de alocar recursos, validar os dados e usar um decodificador mantido. Miniaturas ausentes ou inválidas devem resultar em uma imagem substituta, sem impedir o uso dos estados.

Ao portar, adaptar as dimensões e o recorte ao framebuffer de cada sistema e preservar os limites e validações de leitura.
