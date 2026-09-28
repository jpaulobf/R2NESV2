#pragma once

#include <SDL.h>
#include <cstdint>
#include <array>
#include "imgui.h"

namespace R2NES::Core
{
    /**
     * @brief Visualizador de paletas do PPU.
     *
     * Exibe as 8 paletas (4 de background + 4 de sprites) com cores extraídas
     * do `systemPalette`. A janela possui seu próprio contexto ImGui e renderer
     * para operar de forma independente da UI principal.
     */
    class PaletteViewer
    {
    public:
        /** Cria a instância do visualizador (não abre a janela). */
        PaletteViewer();
        /** Destrói o visualizador e libera recursos SDL/ImGui se alocados. */
        ~PaletteViewer();

        /**
         * @brief Abre (ou mostra) a janela do visualizador posicionada ao lado do pai.
         * @param parentX Posição X do pai.
         * @param parentY Posição Y do pai.
         * @param parentW Largura do pai (offset horizontal).
         */
        void open(int parentX, int parentY, int parentW);

        /** Oculta a janela do visualizador (recursos mantidos para reuso). */
        void close();

        /**
         * @brief Renderiza as paletas a partir da tabela de paletas do PPU.
         * @param paletteTable Buffer de 32 bytes com os índices de cor.
         * @param systemPalette Tabela de cores do sistema (ARGB u32).
         */
        void render(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette);

        /** Retorna true enquanto a janela estiver visível. */
        bool isOpen() const { return visible; }

        /** Retorna o id SDL da janela (0 se não criada). */
        uint32_t getWindowID() const;

        /** Encaminha eventos SDL para o contexto ImGui desta janela. */
        void handleEvent(SDL_Event *e);

        /** Atualiza a posição da janela para acompanhar o pai. */
        void updatePosition(int parentX, int parentY, int parentW);

    private:
        SDL_Window *window = nullptr;         /**< Janela SDL proprietária do visualizador */
        SDL_Renderer *renderer = nullptr;     /**< Renderer SDL associado */
        ImGuiContext *imguiContext = nullptr; /**< Contexto ImGui separado */
        bool visible = false;                 /**< Indica se a janela está visível */
    };
}