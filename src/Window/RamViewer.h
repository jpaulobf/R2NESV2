#pragma once

#include <SDL.h>
#include <cstdint>
#include <string>
#include "imgui.h"

namespace R2NES::Core
{
    class RAM;

    /**
     * @brief Visualizador da RAM principal (Work RAM) com edição in-place.
     *
     * Fornece uma janela de depuração que permite visualizar um intervalo de
     * endereços da RAM, ver valores em hexadecimal e ASCII, e editar bytes
     * manualmente. Possui um contexto ImGui e renderer próprios para operar
     * independentemente da UI principal.
     */
    class RamViewer
    {
    public:
        /** Cria a instância (não abre a janela). */
        RamViewer();
        /** Destrói a instância e libera recursos SDL/ImGui se alocados. */
        ~RamViewer();

        /**
         * @brief Abre (ou mostra) a janela do visualizador.
         * @param parentX Posição X da janela pai.
         * @param parentY Posição Y da janela pai.
         * @param parentW Largura da janela pai (usada como offset).
         */
        void open(int parentX, int parentY, int parentW);

        /** Oculta a janela do visualizador (recursos mantidos para reuso). */
        void close();

        /**
         * @brief Renderiza o conteúdo da RAM fornecida.
         * @param ram Ponteiro para o objeto `RAM` a ser visualizado.
         */
        void render(RAM *ram);

        /** Encaminha eventos SDL para o ImGui do visualizador. */
        void handleEvent(SDL_Event *e);

        /** Atualiza a posição da janela para acompanhar o pai. */
        void updatePosition(int parentX, int parentY, int parentW);

        /** Retorna true enquanto a janela estiver visível. */
        bool isOpen() const { return visible; }

        /** Retorna o id SDL da janela (0 se não criada). */
        uint32_t getWindowID() const;

    private:
        SDL_Window *window = nullptr;         /**< Janela SDL do visualizador */
        SDL_Renderer *renderer = nullptr;     /**< Renderer SDL */
        bool visible = false;                 /**< Visibilidade atual da janela */
        ImGuiContext *imguiContext = nullptr; /**< Contexto ImGui separado */
    };
}