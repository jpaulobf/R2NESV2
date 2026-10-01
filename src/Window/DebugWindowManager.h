#pragma once

#include <SDL.h>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include "Disassembler.h"
#include "OamViewer.h"
#include "PaletteViewer.h"
#include "RamViewer.h"
#include "TileViewer.h"
#include "VRamViewer.h"

namespace R2NES::Core
{
    class RAM;
    class VRAM;

    // Identifica a janela de depuração afetada por um evento de fechamento.
    enum class DebugWindow
    {
        None,         // Nenhum viewer corresponde ao identificador SDL recebido.
        Tile,         // Viewer das pattern tables.
        Palette,      // Viewer da paleta atual da PPU.
        Ram,          // Viewer da RAM principal.
        Disassembler, // Viewer do desassembly da CPU.
        Oam,          // Viewer da memória de atributos de sprites.
        Vram          // Viewer da VRAM da PPU.
    };

    // Centraliza a posse e o ciclo de vida das janelas auxiliares de depuração.
    class DebugWindowManager
    {
    public:
        // Encaminha eventos SDL a todos os viewers ativos.
        void handleEvent(SDL_Event *event);

        // Fecha o viewer associado ao identificador SDL e informa qual foi fechado.
        DebugWindow closeWindow(uint32_t windowId);

        // Fecha viewers específicos individualmente
        void closeTileViewer() { tileViewer.close(); }
        void closePaletteViewer() { paletteViewer.close(); }
        void closeRamViewer() { ramViewer.close(); }
        void closeDisassembler() { disassembler.close(); }
        void closeOamViewer() { oamViewer.close(); }
        void closeVramViewer() { vramViewer.close(); }

        // Fecha todos os viewers de depuração.
        void closeAll();

        // Reposiciona os viewers relativos à janela principal.
        void updatePositions(int parentX, int parentY, int parentWidth);

        // Abre cada viewer posicionado em relação à janela principal.
        void openTileViewer(int parentX, int parentY, int parentWidth);
        void openPaletteViewer(int parentX, int parentY, int parentWidth);
        void openRamViewer(int parentX, int parentY, int parentWidth);
        void openDisassembler(int parentX, int parentY, int parentWidth);
        void openOamViewer(int parentX, int parentY, int parentWidth);
        void openVramViewer(int parentX, int parentY, int parentWidth);

        // Encaminha os dados atuais da emulação ao viewer correspondente.
        void updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1);
        void updatePaletteViewer(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette);
        void updateRamViewer(RAM *ram);
        void updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status);
        void updateOamViewer(const std::array<uint8_t, 256> &oam);
        void updateVramViewer(VRAM *vram);

        // Informa se cada janela auxiliar está aberta.
        bool isTileViewerOpen() const;
        bool isPaletteViewerOpen() const;
        bool isRamViewerOpen() const;
        bool isDisassemblerOpen() const;
        bool isOamViewerOpen() const;
        bool isVramViewerOpen() const;

    private:
        // Os viewers são propriedade exclusiva deste coordenador.
        TileViewer tileViewer;
        PaletteViewer paletteViewer;
        RamViewer ramViewer;
        Disassembler disassembler;
        OamViewer oamViewer;
        VRamViewer vramViewer;
    };
}