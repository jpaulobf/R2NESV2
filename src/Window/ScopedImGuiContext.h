#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include "imgui.h"

namespace R2NES::Core
{
    /**
     * @brief Utilitário RAII para salvar o contexto ImGui atual e restaurá-lo
     * automaticamente ao sair do escopo. Essencial para evitar que subjanelas
     * (viewers) corrompam o contexto ImGui da janela principal ou entre si.
     */
    class ScopedImGuiContext
    {
    public:
        explicit ScopedImGuiContext(ImGuiContext *newContext)
            : previousContext(ImGui::GetCurrentContext())
        {
            if (newContext)
                ImGui::SetCurrentContext(newContext);
        }

        ~ScopedImGuiContext()
        {
            ImGui::SetCurrentContext(previousContext);
        }

        ScopedImGuiContext(const ScopedImGuiContext &) = delete;
        ScopedImGuiContext &operator=(const ScopedImGuiContext &) = delete;

    private:
        ImGuiContext *previousContext = nullptr;
    };

    /**
     * @brief Retorna o identificador de janela (windowID) de um evento SDL,
     * ou 0 se o evento não estiver associado a uma janela específica.
     */
    inline uint32_t getEventWindowID(const SDL_Event *e)
    {
        if (!e)
            return 0;
        if (e->type >= SDL_EVENT_WINDOW_FIRST && e->type <= SDL_EVENT_WINDOW_LAST)
            return e->window.windowID;
        switch (e->type)
        {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            return e->key.windowID;
        case SDL_EVENT_TEXT_EDITING:
            return e->edit.windowID;
        case SDL_EVENT_TEXT_INPUT:
            return e->text.windowID;
        case SDL_EVENT_MOUSE_MOTION:
            return e->motion.windowID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return e->button.windowID;
        case SDL_EVENT_MOUSE_WHEEL:
            return e->wheel.windowID;
        default:
            return 0;
        }
    }
}