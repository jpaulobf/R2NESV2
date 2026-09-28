#pragma once

#include <SDL.h>
#include <cstdint>
#include <array>
#include "imgui.h"

namespace R2NES::Core
{
    /**
     * @brief Visualizador de OAM (Object Attribute Memory).
     *
     * Esta classe fornece uma janela de depuração que exibe o conteúdo do
     * OAM da PPU do NES em uma tabela legível. Ela possui um `SDL_Window`,
     * `SDL_Renderer` e um contexto ImGui dedicado, permitindo que a janela do
     * visualizador seja mostrada/ocultada independentemente da UI principal.
     *
     * Observações:
     * - O método `render()` espera um snapshot de 256 bytes disposto como 64
     *   entradas de 4 bytes cada: (Y, Tile, Attr, X).
     * - SDL e ImGui não são thread-safe; todos os métodos públicos devem ser
     *   chamados a partir da thread principal da aplicação/UI.
     */
    class OamViewer
    {
    public:
        /** Cria uma instância do visualizador de OAM. Não abre a janela. */
        OamViewer();
        /** Destrói o visualizador e libera recursos SDL/ImGui, se criados. */
        ~OamViewer();

        /**
         * @brief Cria (ou mostra) a janela do visualizador posicionada ao lado
         * do window pai.
         * @param parentX Posição X do pai.
         * @param parentY Posição Y do pai.
         * @param parentW Largura do pai (utilizada como offset horizontal).
         */
        void open(int parentX, int parentY, int parentW);

        /** Oculta a janela do visualizador. Recursos internos são mantidos para reutilização. */
        void close();

        /**
         * @brief Renderiza a UI do visualizador para o snapshot de OAM fornecido.
         * @param oam Buffer de 256 bytes do OAM (64 sprites × 4 bytes: Y, Tile, Attr, X).
         *
         * A função desenha uma tabela ImGui representando cada sprite. Não modifica o buffer.
         */
        void render(const std::array<uint8_t, 256> &oam);

        /** Encaminha eventos SDL para o backend ImGui desta janela. */
        void handleEvent(SDL_Event *e);

        /**
         * @brief Atualiza a posição da janela para que ela acompanhe o pai.
         * @param parentX Posição X do pai.
         * @param parentY Posição Y do pai.
         * @param parentW Largura do pai.
         */
        void updatePosition(int parentX, int parentY, int parentW);

        /** Retorna true enquanto o visualizador estiver visível. */
        bool isOpen() const { return visible; }

        /** Retorna o id da janela SDL deste visualizador (0 se não criado). */
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;         /**< SDL window owned by this viewer */
        SDL_Renderer *renderer = nullptr;     /**< SDL renderer for `window` */
        ImGuiContext *imguiContext = nullptr; /**< Independent ImGui context */
        bool visible = false;                 /**< Whether the window is currently shown */

        /** Representation of a single sprite entry in OAM (Y, Tile, Attr, X). */
        struct SpriteData
        {
            uint8_t y;     /**< Y coordinate (NES semantics) */
            uint8_t tile;  /**< Tile index */
            uint8_t attrs; /**< Attribute byte (palette, flips, priority) */
            uint8_t x;     /**< X coordinate */
        };
    };
}