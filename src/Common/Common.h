#pragma once

namespace R2NES::Core
{
    // Modos de espelhamento (mirroring) usados pelo PPU para mapear bancos de nametables.
    // - HORIZONTAL: espelhamento horizontal clássico (nomeTABLEs 0/1 e 2/3 espelhados)
    // - VERTICAL: espelhamento vertical (0/2 e 1/3 espelhados)
    // - ONESCREEN_LO / ONESCREEN_HI: usa apenas uma nametable (parte baixa/alta)
    // - FOUR_SCREEN: quatro nametables independentes (MMC mappers que suportam)
    enum class MirrorMode
    {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LO,
        ONESCREEN_HI,
        FOUR_SCREEN
    };

    // Estado do mouse mantido pela camada de UI/Window para delegação a visualizadores
    struct MouseState
    {
        int x = -1;              // Posição X do cursor (pixels, ou -1 quando inválido)
        int y = -1;              // Posição Y do cursor (pixels, ou -1 quando inválido)
        bool leftButton = false; // Estado do botão esquerdo do mouse
    };

    // Modos de exibição da janela principal
    // - WINDOWED: janela padrão
    // - FULLSCREEN_STRETCH: fullscreen com stretching para preencher a tela
    // - FULLSCREEN_ASPECT_8_7: fullscreen mantendo a proporção 8:7 (NES aspect)
    enum class DisplayMode
    {
        WINDOWED,
        FULLSCREEN_STRETCH,
        FULLSCREEN_ASPECT_8_7
    };

    // Tipos de paletas (algoritmos de conversão de cores / filtros)
    // - DEFAULT: paleta padrão do emulador
    // - SMOOTH: paleta com suavização/tonalidade
    // - NESTOPIA: paleta compatível com o NESTOPIA
    // - WAVEBEAM: paleta estilo WaveBeam
    // - NEON: paleta com alto contraste e saturação (efeito neon)
    enum class PaletteType
    {
        DEFAULT,
        SMOOTH,
        NESTOPIA,
        WAVEBEAM,
        NEON
    };

    // Tipos de shaders/pós-processamento aplicáveis ao output de vídeo
    // - NONE: sem shader
    // - SCANLINES: linhas de varredura
    // - CRT: emulação de CRT 2D
    // - CRT3D: emulação de CRT com profundidade 3D
    // - SCALEFX: efeitos de escala (zoom/filtragem)
    // - XBRZMULTI: algoritmo xBRZ multi-scale para upscaling de pixels
    enum class ShaderType
    {
        NONE,
        SCANLINES,
        CRT,
        CRT3D,
        SCALEFX,
        XBRZMULTI
    };
}