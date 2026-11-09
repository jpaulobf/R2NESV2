# Shaders: notas para R2NESV2, R2BOY e R2SEGA8

Display > Shaders > Gauss Scanlines executa no Windows o preset
`ntsc/ntsc-256px-gauss-scanline.cgp`, com seus quatro passes Cg. O preset
experimental `xbr/super-xbr-2p.cgp` usa dois passes super-xBR e um de
redimensionamento Jinc. O Super 4xbr 3D 6p smoother usa
`xbr/super-4xbr-3d-6p-smoother-nes.cgp`: uma ampliação inicial 4x com
vizinho mais próximo, seis passes de efeito e um stock final. O preset
original de sete passes continua em `super-4xbr-3d-6p-smoother.cgp`.
Os dois xBR foram retirados dos menus após avaliação visual; os arquivos,
a integração interna e os testes foram mantidos para referência.
O filtro anterior Display > Scanlines é independente e pode ser usado em conjunto.
Display > Shaders > HQ2 usa o HQ2x de um passe do pacote `hqx`, sua tabela
`resources/hq2x.png` e um passe final de cópia para o viewport. A versão de
um passe é uma prova visual; o pacote também contém uma variante multipasse.
Display > Shaders > ScaleFX usa os cinco passes de `scalefx.cgp`, com uma
cópia final para o viewport em `scalefx-visual.cgp`. Os dois primeiros passes
usam texturas de ponto flutuante; o resultado seleciona cores da imagem de
origem e é gerado em 3x antes da cópia final.

`ShaderPreset` lê o subconjunto de `.cgp` usado pelo preset, verifica os
arquivos e includes e planeja as dimensões dos passes. `NtscShaderPipeline`
expande os includes e compila os programas originais com o compilador HLSL do
Windows; adapta a posição do vértice para um quad em clip space e a constante
global da matriz NTSC para que seja inicializada no shader. Os passes usam
texturas intermediárias, inclusive um primeiro alvo em ponto flutuante,
filtros e contador de quadros conforme o preset. O último passe produz a
imagem no tamanho do viewport. O build copia os shaders para junto do programa.

O backend atual cria um dispositivo Direct3D 11 separado do renderer SDL,
executa os passes e faz leitura de volta para uma textura do SDL antes da
interface. Isso facilita a prova de conceito e mantém o renderer existente,
mas introduz sincronização e transferência por quadro, podendo ser caro em
alta resolução. Uma integração futura deve compartilhar a textura com o
renderer ou usar SDL GPU para evitar a cópia. O dispositivo tenta GPU e depois
WARP. Sem Direct3D 11 ou compilador disponível, a seleção informa o erro e a
imagem continua normal. Em Linux e outros sistemas o efeito ainda não está
disponível. Revalidar desempenho, proporção, crop e fullscreen em GPUs reais.

O segundo passe super-xBR também recebe a textura original como PASSPREV2;
o preset de seis passes usa a mesma imagem por meio de ORIG.
O ScaleFX também acessa um passe anterior com PASSPREV2 e o framebuffer
original com PASSPREV5; a referência é resolvida de acordo com o passe atual.
As declarações `#pragma parameter` fornecem os valores padrão compartilhados
pelos passes. Seus shaders usam uma interface Cg mais antiga, adaptada em
memória para compilação HLSL sem modificar os arquivos originais.

O leitor rejeita opções desconhecidas e o backend foi adaptado aos presets
selecionados. O HQ2 usa uma textura LUT carregada por Windows Imaging Component
e amostrada sem interpolação; não há suporte geral a parâmetros, outras
texturas extras, quadros anteriores ou
presets arbitrários. Para portar aos outros emuladores, adaptar a origem e a
área de saída e manter o efeito antes do desenho da interface. Novos presets
podem exigir recursos adicionais, revisão de licenças e outros backends.

Os testes `shader-preset`, `ntsc-shader-pipeline`, `xbr-shader-pipeline` e
`xbr-6p-shader-pipeline` verificam a leitura, os
arquivos, a escala, a execução dos quatro passes, a mudança de fase por quadro
do NTSC, o redimensionamento, a cor uniforme e a suavização de diagonais. A validação
visual com jogos continua necessária.
O teste `hq2-shader-pipeline` verifica a saída do HQ2 com sua LUT, incluindo
detalhes diagonais, cor uniforme e redimensionamento. A implementação foi
compilada e testada, mas ainda aguarda avaliação visual com jogos.
O teste `scalefx-shader-pipeline` verifica também a preservação das cores da
imagem de origem. A avaliação visual e de desempenho com jogos ainda falta.

## Estado e orientações para a próxima rodada

- A intenção é oferecer cerca de quatro ou cinco presets fixos no menu, não
  um carregador genérico de shaders para o usuário. A seleção definitiva dos
  presets ainda depende de comparação visual e de desempenho.
- O usuário confirmou que Gauss Scanlines funcionou muito bem no Windows.
  Os dois presets xBR foram avaliados e removidos do menu. O usuário relatou uma
  imagem muito borrada no 6p original: esse shader foi projetado para uma
  entrada 3D já ampliada em 4x, mas recebia a imagem nativa do NES. Foi
  adicionada uma ampliação inicial de 4x. O teste automatizado
  verifica o contraste de traços de um pixel e falha com o preset anterior.
- A implementação e os recursos estão no workspace do R2NESV2. No estado
  atual vários arquivos, inclusive este documento, a pasta `shaders`, o
  pipeline, o leitor de presets e seus testes, ainda não estão rastreados pelo
  Git. Não presumir que já foram commitados ou copiados aos outros emuladores.
- Para portar, consultar `Window.cpp`/`Window.h` para seleção, recorte,
  dimensões do destino, apresentação e mensagens de falha; `Common.h` para os
  identificadores; `ShaderPreset` e `NtscShaderPipeline` para leitura e GPU;
  e `CMakeLists.txt` para bibliotecas Direct3D, cópia dos recursos e testes.
  O nome `NtscShaderPipeline` é histórico: ele também executa os dois xBR.
- Ajustar as dimensões nativas, formato e pitch do framebuffer de cada
  emulador. Testar recorte, redimensionamento, proporção, fullscreen, pausa,
  troca de preset e falhas. Preservar os filtros e menus existentes de cada
  projeto e manter a interface fora do processamento do jogo.
- A pasta completa de shaders foi mantida para experimentação. Antes de uma
  distribuição, selecionar presets e suas dependências recursivas, conferir
  licenças e avisos dos arquivos escolhidos e decidir como empacotá-los.
- Os testes atuais exercitam a execução Direct3D e a integração SDL por
  software; não substituem comparação visual e medição de FPS com jogos.

Referências: https://docs.libretro.com/development/shader/shader-overview/ e
https://github.com/libretro/common-shaders.
