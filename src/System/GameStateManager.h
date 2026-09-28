#pragma once

#include <string>
#include <map>
#include <SDL.h>

namespace R2NES::Core
{
    class NES;
}
namespace R2NES::Core
{
    class Window;
}

namespace R2NES::System
{
    // Coordena o ciclo de vida da ROM, save states e o cache do desassembly da CPU.
    class GameStateManager
    {
    public:
        // Não possui recursos externos; o estado é mantido pelas strings e pelo cache interno.
        GameStateManager() = default;
        ~GameStateManager() = default;

        // Carrega uma ROM, reinicializa o NES e sincroniza a interface com o novo cartucho.
        void loadRom(const std::string &path, Core::NES &nes, Core::Window &window);

        // Remove o cartucho atual e limpa o estado de interface e dados derivados.
        void unloadRom(Core::NES &nes, Core::Window &window);

        // Reinicia o NES e confirma na interface que a solicitação foi atendida.
        void reset(Core::NES &nes, Core::Window &window);

        // Processa uma solicitação pendente de salvar ou carregar o estado da ROM atual.
        void handleSaveLoadState(Core::NES &nes, Core::Window &window);

        // Expõe o desassembly em cache consumido pelo viewer de depuração.
        const std::map<uint16_t, std::string> &getCachedDisassembly() const { return cachedDisassembly; }

        // Permite recarregar o disassembly caso o banco de memória mude
        void updateDisassemblyCache(Core::NES &nes, uint16_t currentPC);

    private:
        // Caminho da ROM que define o nome dos arquivos de save state.
        std::string currentRomPath;

        // Mapeia endereços da CPU para instruções, evitando reconstrução a cada quadro.
        std::map<uint16_t, std::string> cachedDisassembly;
    };
}
