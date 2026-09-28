#include "DebugWindowManager.h"

namespace R2NES::Core
{
    // Distribui eventos SDL para que cada contexto ImGui auxiliar responda à sua janela.
    void DebugWindowManager::handleEvent(SDL_Event *event)
    {
        tileViewer.handleEvent(event);
        paletteViewer.handleEvent(event);
        ramViewer.handleEvent(event);
        disassembler.handleEvent(event);
        oamViewer.handleEvent(event);
        vramViewer.handleEvent(event);
    }

    // Localiza o viewer pelo ID SDL, fecha-o e devolve sua categoria ao menu chamador.
    DebugWindow DebugWindowManager::closeWindow(uint32_t windowId)
    {
        if (windowId != 0 && windowId == tileViewer.getWindowID())
        {
            tileViewer.close();
            return DebugWindow::Tile;
        }
        if (windowId != 0 && windowId == paletteViewer.getWindowID())
        {
            paletteViewer.close();
            return DebugWindow::Palette;
        }
        if (windowId != 0 && windowId == ramViewer.getWindowID())
        {
            ramViewer.close();
            return DebugWindow::Ram;
        }
        if (windowId != 0 && windowId == disassembler.getWindowID())
        {
            disassembler.close();
            return DebugWindow::Disassembler;
        }
        if (windowId != 0 && windowId == oamViewer.getWindowID())
        {
            oamViewer.close();
            return DebugWindow::Oam;
        }
        if (windowId != 0 && windowId == vramViewer.getWindowID())
        {
            vramViewer.close();
            return DebugWindow::Vram;
        }

        return DebugWindow::None;
    }

    // Encerra todas as janelas auxiliares ao resetar ou descarregar a ROM.
    void DebugWindowManager::closeAll()
    {
        tileViewer.close();
        paletteViewer.close();
        ramViewer.close();
        disassembler.close();
        oamViewer.close();
        vramViewer.close();
    }

    // Mantém os viewers ancorados na posição atual da janela principal.
    void DebugWindowManager::updatePositions(int parentX, int parentY, int parentWidth)
    {
        tileViewer.updatePosition(parentX, parentY, parentWidth);
        paletteViewer.updatePosition(parentX, parentY, parentWidth);
        ramViewer.updatePosition(parentX, parentY, parentWidth);
        disassembler.updatePosition(parentX, parentY, parentWidth);
        oamViewer.updatePosition(parentX, parentY, parentWidth);
        vramViewer.updatePosition(parentX, parentY, parentWidth);
    }

    // Abre o viewer das pattern tables ao lado da janela principal.
    void DebugWindowManager::openTileViewer(int parentX, int parentY, int parentWidth)
    {
        tileViewer.open(parentX, parentY, parentWidth);
    }

    // Abre o viewer da paleta atual da PPU.
    void DebugWindowManager::openPaletteViewer(int parentX, int parentY, int parentWidth)
    {
        paletteViewer.open(parentX, parentY, parentWidth);
    }

    // Abre o viewer da RAM principal.
    void DebugWindowManager::openRamViewer(int parentX, int parentY, int parentWidth)
    {
        ramViewer.open(parentX, parentY, parentWidth);
    }

    // Abre o viewer do desassembly da CPU.
    void DebugWindowManager::openDisassembler(int parentX, int parentY, int parentWidth)
    {
        disassembler.open(parentX, parentY, parentWidth);
    }

    // Abre o viewer da memória de atributos de sprites.
    void DebugWindowManager::openOamViewer(int parentX, int parentY, int parentWidth)
    {
        oamViewer.open(parentX, parentY, parentWidth);
    }

    // Abre o viewer da VRAM da PPU.
    void DebugWindowManager::openVramViewer(int parentX, int parentY, int parentWidth)
    {
        vramViewer.open(parentX, parentY, parentWidth);
    }

    // Atualiza as duas pattern tables renderizadas pelo Tile Viewer.
    void DebugWindowManager::updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        tileViewer.render(pixels0, pixels1);
    }

    // Atualiza os índices da paleta PPU e as cores do preset ativo.
    void DebugWindowManager::updatePaletteViewer(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette)
    {
        paletteViewer.render(paletteTable, systemPalette);
    }

    // Atualiza o conteúdo exibido da RAM da CPU.
    void DebugWindowManager::updateRamViewer(RAM *ram)
    {
        ramViewer.render(ram);
    }

    // Atualiza instruções, registradores e controles de execução do desassembly.
    void DebugWindowManager::updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                                bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status)
    {
        disassembler.render(pc, disassembly, stepByStep, stepRequested, a, x, y, stkp, status);
    }

    // Atualiza os 256 bytes da OAM que descrevem os sprites.
    void DebugWindowManager::updateOamViewer(const std::array<uint8_t, 256> &oam)
    {
        oamViewer.render(oam);
    }

    // Atualiza o conteúdo exibido da VRAM da PPU.
    void DebugWindowManager::updateVramViewer(VRAM *vram)
    {
        vramViewer.render(vram);
    }

    // Expõe o estado de visibilidade de cada viewer à Window e ao menu nativo.
    bool DebugWindowManager::isTileViewerOpen() const { return tileViewer.isOpen(); }
    bool DebugWindowManager::isPaletteViewerOpen() const { return paletteViewer.isOpen(); }
    bool DebugWindowManager::isRamViewerOpen() const { return ramViewer.isOpen(); }
    bool DebugWindowManager::isDisassemblerOpen() const { return disassembler.isOpen(); }
    bool DebugWindowManager::isOamViewerOpen() const { return oamViewer.isOpen(); }
    bool DebugWindowManager::isVramViewerOpen() const { return vramViewer.isOpen(); }
}