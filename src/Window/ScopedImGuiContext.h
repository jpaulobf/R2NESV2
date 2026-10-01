#pragma once

#include <SDL.h>
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
            if (previousContext)
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
        switch (e->type)
        {
        case SDL_WINDOWEVENT:
            return e->window.windowID;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return e->key.windowID;
        case SDL_TEXTEDITING:
            return e->edit.windowID;
        case SDL_TEXTINPUT:
            return e->text.windowID;
        case SDL_MOUSEMOTION:
            return e->motion.windowID;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return e->button.windowID;
        case SDL_MOUSEWHEEL:
            return e->wheel.windowID;
        default:
            return 0;
        }
    }
}
