#include "Window.h"
#include "ScopedImGuiContext.h"
#include "NtscShaderPipeline.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include <filesystem>
#include "imgui_impl_sdlrenderer3.h"
#include "Util/ConfigManager.h"
#include "Common/Common.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace R2NES::Core
{
    Window::SDLSession::SDLSession()
        : initialized(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
    }

    Window::SDLSession::~SDLSession()
    {
        SDL_Quit();
    }

    void Window::showBatteryMessage(const std::string &message)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Bateria do cartucho", message.c_str(), window);
    }

    bool Window::confirmBatteryImport(const std::string &message)
    {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Manter original"},
            {0, 1, "Importar com backup"}};
        const SDL_MessageBoxData dialog{SDL_MESSAGEBOX_WARNING, window, "Importar bateria MMC5",
                                        message.c_str(), 2, buttons, nullptr};
        int selected = 0;
        return SDL_ShowMessageBox(&dialog, &selected) && selected == 1;
    }

    // Inicializa SDL, renderer, textura de vídeo e contexto ImGui da janela principal.
    Window::Window(const std::string &title, int w, int h, int s)
        : width(w), height(h), scale(s)
    {
        // Store initial windowed state
        lastWindowedW = w * s;
        lastWindowedH = h * s;

        if (!sdlSession.initialized)
        {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }
        window = SDL_CreateWindow(
            title.c_str(),
            w * scale, h * scale,
            0);

        if (!window)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }

        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer)
        {
            std::cerr << "Renderer could not be created: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }
        applyVSync(vsyncEnabled);

        // Garante que o fundo da janela principal também comece preto
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);

        // Criamos a textura no formato ARGB8888 para bater com o frameBuffer da PPU
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            w, h);
        if (!texture)
        {
            std::cerr << "Texture could not be created: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }
        SDL_SetTextureScaleMode(texture, bilinearFiltering ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        imguiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(imguiContext);
        if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer) || !ImGui_ImplSDLRenderer3_Init(renderer))
        {
            std::cerr << "ImGui could not initialize: " << SDL_GetError() << std::endl;
            closed = true;
            return;
        }

        // Redimensiona a janela para 3x por padrão.
        this->windowResize(currentWindowX);

        // Informa a Engine o estado inicial do VSync
        if (vsyncCallback)
            this->vsyncCallback(vsyncEnabled);
    }

    // Fecha controles e libera recursos de ImGui, SDL e suas superfícies gráficas.
    Window::~Window()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (controllers[i])
            {
                SDL_CloseGamepad(controllers[i]);
                controllers[i] = nullptr;
            }
        }

        if (imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            if (ImGui::GetIO().BackendRendererUserData)
                ImGui_ImplSDLRenderer3_Shutdown();
            if (ImGui::GetIO().BackendPlatformUserData)
                ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext(imguiContext);
        }

        SDL_DestroyTexture(shaderTexture);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    void Window::setKeyboardMappingCallback(std::function<void(const R2Keyboard::Mapping &)> callback)
    {
        keyboardMappingCallback = std::move(callback);
        keyboardDialog.setMapping(keyboardConfig.load());
        if (keyboardMappingCallback)
            keyboardMappingCallback(keyboardDialog.mapping());
    }

    int Window::playerForGamepad(int slot) const
    {
        for (int player = 0; player < 2; ++player)
            if (playerJoystick[player] && slot-- == 0)
                return player + 1;
        return 0;
    }

    void Window::releaseGamepad(int player)
    {
        if (!player)
            return;
        if (controllerCallback)
            for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
                controllerCallback(player, static_cast<SDL_GamepadButton>(button), false);
        if (controllerTriggerCallback)
        {
            controllerTriggerCallback(player, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, false);
            controllerTriggerCallback(player, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, false);
        }
    }

    void Window::setPlayerJoystick(int player, bool enabled)
    {
        if (player < 1 || player > 2 || playerJoystick[player - 1] == enabled)
            return;
        // A mudança pode deslocar também o controle atribuído ao outro jogador.
        for (int slot = 0; slot < 2; ++slot)
            releaseGamepad(playerForGamepad(slot));
        playerJoystick[player - 1] = enabled;
        if (playerControllerCallback)
            playerControllerCallback(player, enabled);
    }

    // Distribui eventos SDL entre ImGui, controles, mouse, teclado, menus e janelas auxiliares.
    void Window::pollEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            // A captura do diálogo tem prioridade sobre os atalhos e a entrada de jogo.
            if (keyboardDialog.processEvent(e, SDL_GetWindowID(window)))
                continue;
            // Processa eventos para o ImGui da janela principal se o evento pertencer a ela
            if (imguiContext)
            {
                uint32_t winId = getEventWindowID(&e);
                if (winId == 0 || winId == SDL_GetWindowID(window))
                {
                    ImGui::SetCurrentContext(imguiContext);
                    ImGui_ImplSDL3_ProcessEvent(&e);
                }
            }

            // Encaminha o evento para os viewers que possuem contexto ImGui próprio.
            debugWindowManager.handleEvent(&e);

            if (e.type == SDL_EVENT_QUIT)
                closed = true;

            if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST && e.window.windowID == SDL_GetWindowID(window))
            {
                if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
                    releaseMenuPause(true);
                else if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                    menuPauseSuspendedUntilClose = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN && e.key.windowID == SDL_GetWindowID(window) && !e.key.repeat &&
                ((e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_CTRL) && !(e.key.mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT | SDL_KMOD_GUI))) ||
                 (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT) && !(e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_GUI)))))
            {
                if (currentDisplayMode == DisplayMode::WINDOWED)
                    windowBorderlessFullscreen();
                else
                {
                    restoreFullscreenWindow();
                }
                continue;
            }
            // Sai do modo tela cheia e restaura o menu ao apertar ESC
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.windowID == SDL_GetWindowID(window) && e.key.key == SDLK_ESCAPE)
            {
                if (currentDisplayMode != DisplayMode::WINDOWED)
                {
                    restoreFullscreenWindow();
                }
            }

            // Gerenciamento de Conexão de Controles
            if (e.type == SDL_EVENT_GAMEPAD_ADDED)
            {
                const SDL_JoystickID deviceId = e.gdevice.which;
                bool alreadyOpened = false;
                for (auto *controller : controllers)
                    if (controller && SDL_GetGamepadID(controller) == deviceId)
                        alreadyOpened = true;
                if (!alreadyOpened && SDL_IsGamepad(deviceId))
                {
                    for (int i = 0; i < 2; ++i)
                    {
                        if (!controllers[i])
                        {
                            controllers[i] = SDL_OpenGamepad(deviceId);
                            if (controllers[i])
                                std::cout << "Controller connected in slot " << (i + 1) << ": "
                                          << SDL_GetGamepadName(controllers[i]) << std::endl;
                            break;
                        }
                    }
                }
            }
            else if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
            {
                for (int i = 0; i < 2; ++i)
                {
                    if (controllers[i] && SDL_GetGamepadID(controllers[i]) == e.gdevice.which)
                    {
                        for (int slot = 0; slot < 2; ++slot)
                            releaseGamepad(playerForGamepad(slot));
                        SDL_CloseGamepad(controllers[i]);
                        if (i == 0)
                            controllers[0] = controllers[1];
                        controllers[1] = nullptr;
                        int count = 0;
                        SDL_JoystickID *available = SDL_GetGamepads(&count);
                        for (int device = 0; device < count && !controllers[1]; ++device)
                            if (!controllers[0] || SDL_GetGamepadID(controllers[0]) != available[device])
                                controllers[controllers[0] ? 1 : 0] = SDL_OpenGamepad(available[device]);
                        SDL_free(available);
                        break;
                    }
                }
            }

            // Eventos de Botões do Controle
            else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
            {
                if (controllerCallback)
                {
                    for (int i = 0; i < 2; ++i)
                        if (controllers[i] && SDL_GetGamepadID(controllers[i]) == e.gbutton.which)
                            if (const int player = playerForGamepad(i))
                                controllerCallback(player, static_cast<SDL_GamepadButton>(e.gbutton.button), e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                }
            }

            // Eventos de Eixo do Controle (ex: gatilhos L2/R2 que aparecem como eixos)
            else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
            {
                // Threshold para considerar o gatilho "pressionado"
                const int TRIGGER_THRESHOLD = 16000;

                if (controllerTriggerCallback)
                {
                    for (int i = 0; i < 2; ++i)
                        if (controllers[i] && SDL_GetGamepadID(controllers[i]) == e.gaxis.which)
                            if (const int player = playerForGamepad(i))
                                controllerTriggerCallback(player, static_cast<SDL_GamepadAxis>(e.gaxis.axis), e.gaxis.value > TRIGGER_THRESHOLD);
                }
            }

            // Captura de Mouse para Zapper
            if ((e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                getEventWindowID(&e) == SDL_GetWindowID(window))
            {
                float mouseX, mouseY;
                mouseState.leftButton = (SDL_GetMouseState(&mouseX, &mouseY) & SDL_BUTTON_LMASK) != 0;

                int winW, winH;
                SDL_GetWindowSize(window, &winW, &winH);

                if (currentDisplayMode == DisplayMode::FULLSCREEN_ASPECT_8_7)
                {
                    // Remove as barras laterais/superiores antes de mapear o mouse para 256x240.
                    double target_aspect = 8.0 / 7.0;
                    int renderW, renderH, offsetX, offsetY;
                    if ((double)winW / winH > target_aspect)
                    {
                        renderH = winH;
                        renderW = (int)(winH * target_aspect);
                    }
                    else
                    {
                        renderW = winW;
                        renderH = (int)(winW / target_aspect);
                    }
                    offsetX = (winW - renderW) / 2;
                    offsetY = (winH - renderH) / 2;

                    mouseState.x = (int)((float)(mouseX - offsetX) * 256.0f / (float)renderW);
                    mouseState.y = (int)((float)(mouseY - offsetY) * 240.0f / (float)renderH);
                }
                else
                {
                    // Em janela ou fullscreen esticado, a escala usa toda a área exibida.
                    mouseState.x = (int)((float)mouseX * 256.0f / (float)winW);
                    mouseState.y = (int)((float)mouseY * 240.0f / (float)winH);
                }

                // Garante que os valores não saiam do range do NES (Clamping)
                mouseState.x = std::max(0, std::min(255, mouseState.x));
                mouseState.y = std::max(0, std::min(239, mouseState.y));
            }

            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
                e.key.windowID == SDL_GetWindowID(window) && e.key.key != SDLK_ESCAPE)
            {
                if (keyCallback)
                {
                    bool isPressed = (e.type == SDL_EVENT_KEY_DOWN);
                    keyCallback(e.key.key, isPressed);
                }
            }

            // Trata o fechamento de janelas individuais
            if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST)
            {
                if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                {
                    if (e.window.windowID == SDL_GetWindowID(window))
                    {
                        closed = true;
                    }
                    else
                    {
                        switch (debugWindowManager.closeWindow(e.window.windowID))
                        {
                        case DebugWindow::Tile:
                        case DebugWindow::Palette:
                        case DebugWindow::Ram:
                        case DebugWindow::Disassembler:
                        case DebugWindow::Oam:
                        case DebugWindow::Vram:
                            break;
                        case DebugWindow::None:
                            break;
                        }
                    }
                }
                else if (e.type == SDL_EVENT_WINDOW_MOVED)
                {
                    // Se a janela principal for movida, reposiciona a Tile Viewer
                    if (e.window.windowID == SDL_GetWindowID(window))
                    {
                        int x, y, w_main, h_main;
                        SDL_GetWindowPosition(window, &x, &y);
                        SDL_GetWindowSize(window, &w_main, &h_main);

                        debugWindowManager.updatePositions(x, y, w_main);
                    }
                    // Update last known windowed position if not in fullscreen
                    if (e.window.windowID == SDL_GetWindowID(window) && currentDisplayMode == DisplayMode::WINDOWED)
                    {
                        lastWindowedX = e.window.data1;
                        lastWindowedY = e.window.data2;
                    }
                }
            }
        }

        // Garante que o contexto ImGui da janela principal permaneça ativo após processar eventos
        if (imguiContext)
            ImGui::SetCurrentContext(imguiContext);
    }

    // Armazena o efeito de pós-processamento escolhido pelo menu.
    void Window::setShader(ShaderType shader)
    {
        if (shader == ShaderType::SCANLINES || shader == ShaderType::HQ2 || shader == ShaderType::SCALEFX || shader == ShaderType::CRT || shader == ShaderType::CRT3D)
        {
            auto &selected = shader == ShaderType::HQ2       ? hq2Shader
                             : shader == ShaderType::SCALEFX ? scaleFxShader
                             : shader == ShaderType::CRT     ? crtShader
                             : shader == ShaderType::CRT3D   ? crt3dShader
                                                             : ntscShader;
            if (!selected)
            {
                auto candidate = std::make_unique<NtscShaderPipeline>();
                std::string error;
                const char *base = SDL_GetBasePath();
                const char *presetName = shader == ShaderType::HQ2       ? "shaders/hqx/hq2x-visual.cgp"
                                         : shader == ShaderType::SCALEFX ? "shaders/scalefx/scalefx-visual.cgp"
                                         : shader == ShaderType::CRT     ? "shaders/crt/crt-lottes.cgp"
                                         : shader == ShaderType::CRT3D   ? "shaders/crt/crt-hyllian-3d-nes.cgp"
                                                                         : "shaders/ntsc/ntsc-256px-gauss-scanline.cgp";
                const char *effectName = shader == ShaderType::HQ2       ? "HQ2"
                                         : shader == ShaderType::SCALEFX ? "ScaleFX"
                                         : shader == ShaderType::CRT     ? "CRT 3D"
                                         : shader == ShaderType::CRT3D   ? "CRT"
                                                                         : "Gauss Scanlines";
                const auto preset = std::filesystem::path(base ? base : "") / presetName;
                if (!candidate->initialize(preset, error))
                {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, effectName, error.c_str(), window);
                    return;
                }
                selected = std::move(candidate);
            }
        }
        this->shader = shader;
    }

    // Mantido para compatibilidade; o menu ImGui consulta o estado a cada quadro.
    void Window::createMenu()
    {
    }

    // Abre o seletor de ROMs implementado com Dear ImGui.
    void Window::openFileDialog()
    {
        std::error_code error;
        const std::string lastRomPath = configManager.getLastRomPath();
        fileDialogDirectory = !lastRomPath.empty() && std::filesystem::is_directory(lastRomPath, error)
                                  ? std::filesystem::path(lastRomPath)
                                  : std::filesystem::current_path(error);
        fileDialogOpen = true;
    }

    // Renderiza o diálogo de seleção de arquivos.
    void Window::renderFileDialog()
    {
        if (!fileDialogOpen)
            return;

        ImGui::OpenPopup("Open ROM");
        bool keepOpen = true;
        if (ImGui::BeginPopupModal("Open ROM", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
#ifdef _WIN32
            const std::string currentDrive = fileDialogDirectory.root_path().string();
            if (ImGui::BeginCombo("Drive", currentDrive.empty() ? "Select drive" : currentDrive.c_str()))
            {
                const DWORD drives = GetLogicalDrives();
                for (int index = 0; index < 26; ++index)
                {
                    if ((drives & (1UL << index)) == 0)
                        continue;

                    const char driveLetter = static_cast<char>('A' + index);
                    const std::string drivePath = std::string(1, driveLetter) + ":\\";
                    const bool isSelected = currentDrive == drivePath;
                    if (ImGui::Selectable(drivePath.c_str(), isSelected))
                        fileDialogDirectory = drivePath;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
#endif
            ImGui::TextUnformatted(fileDialogDirectory.string().c_str());
            ImGui::Separator();
            if (ImGui::Button("Up") && fileDialogDirectory.has_parent_path())
                fileDialogDirectory = fileDialogDirectory.parent_path();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                fileDialogOpen = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::BeginChild("RomFiles", ImVec2(560.0f, 320.0f), true);
            std::error_code error;
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto &entry : std::filesystem::directory_iterator(fileDialogDirectory, error))
                entries.push_back(entry);
            std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right)
                      { return left.path().filename().string() < right.path().filename().string(); });

            for (const auto &entry : entries)
            {
                const auto path = entry.path();
                const std::string name = path.filename().string();
                if (entry.is_directory(error))
                {
                    const std::string label = "[DIR] " + name;
                    if (ImGui::Selectable(label.c_str()))
                        fileDialogDirectory = path;
                    continue;
                }

                std::string extension = path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
                               { return static_cast<char>(std::tolower(character)); });
                if ((extension == ".nes" || extension == ".unf" || extension == ".unif" || extension == ".zip") && ImGui::Selectable(name.c_str()))
                {
                    selectedPath = path.string();
                    configManager.addRomToList(selectedPath);
                    configManager.setLastRomPath(path.parent_path().string());
                    fileDialogOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
        else if (!keepOpen)
        {
            fileDialogOpen = false;
        }
    }

    // Renderiza a barra principal de menus e o navegador de arquivos.
    void Window::renderMenu()
    {
        bool isFullscreen = (currentDisplayMode != DisplayMode::WINDOWED);
        bool openAbout = false;

        if (!isFullscreen && ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open ROM..."))
                    openFileDialog();
                if (ImGui::MenuItem("Reset", "F12", false, cartLoaded))
                    reset();
                if (ImGui::MenuItem("Unload", nullptr, false, cartLoaded))
                    unload();
                ImGui::Separator();
                if (ImGui::MenuItem("Save State", "F5", false, cartLoaded))
                    setSave(true);
                if (ImGui::MenuItem("Load State", "F6", false, cartLoaded))
                    setLoad(true);
                if (ImGui::BeginMenu("Save State Slot", cartLoaded))
                {
                    for (int slot = 1; slot <= 3; ++slot)
                    {
                        const std::string label = "Slot " + std::to_string(slot);
                        if (ImGui::MenuItem(label.c_str(), nullptr, saveSlot == slot))
                            setSaveSlot(slot);
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Recent Files"))
                {
                    // Faz uma cópia da lista para não invalidar iteradores caso a lista seja alterada
                    const auto recentRoms = configManager.getRecentRoms();
                    if (recentRoms.empty())
                        ImGui::TextDisabled("No Recent Files");

                    std::string chosenRom;
                    int index = 0;
                    for (const auto &path : recentRoms)
                    {
                        if (index++ == 10)
                            break;
                        const std::string name = std::filesystem::path(path).filename().string();
                        if (ImGui::MenuItem(name.c_str()))
                        {
                            chosenRom = path;
                        }
                    }

                    // Aplica a alteração fora do loop de iteração
                    if (!chosenRom.empty())
                    {
                        selectedPath = chosenRom;
                        configManager.addRomToList(selectedPath);
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                    closed = true;
                ImGui::EndMenu();
            }

            renderDisplayMenu();

            if (ImGui::BeginMenu("Sound"))
            {
                const char *labels[] = {"Master Sound", "Pulse 1", "Pulse 2", "Triangle", "Noise", "DMC"};
                const bool enabled[] = {soundEnabled, pulse1Enabled, pulse2Enabled, triangleEnabled, noiseEnabled, dmcEnabled};
                const auto setters = {&Window::setSound, &Window::setPulse1, &Window::setPulse2,
                                      &Window::setTriangle, &Window::setNoise, &Window::setDMC};
                ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
                size_t index = 0;
                for (auto setter : setters)
                {
                    if (index)
                        ImGui::Separator();
                    ImGui::PushID(static_cast<int>(index));
                    if (ImGui::MenuItem(labels[index], nullptr, enabled[index]))
                        (this->*setter)(!enabled[index]);
                    ImGui::SetNextItemWidth(160.0f);
                    if (ImGui::SliderInt("Volume", &audioVolumes[index], 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp) && volumeCallback)
                        volumeCallback(index, audioVolumes[index]);
                    ImGui::PopID();
                    ++index;
                }
                ImGui::PopItemFlag();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Input"))
            {
                if (ImGui::MenuItem("Enable Zapper", nullptr, useZapper))
                    setUseZapper(!useZapper);
                ImGui::Separator();
                if (ImGui::BeginMenu("Player Controller"))
                {
                    for (int player = 1; player <= 2; ++player)
                    {
                        if (ImGui::BeginMenu(player == 1 ? "Player 1" : "Player 2"))
                        {
                            if (ImGui::MenuItem("Keyboard", nullptr, !playerJoystick[player - 1]))
                                setPlayerJoystick(player, false);
                            if (ImGui::MenuItem("Joystick", nullptr, playerJoystick[player - 1]))
                                setPlayerJoystick(player, true);
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Use Y/B Buttons (SNES Style)", nullptr, invertBAYB))
                    setInvertBAYB(!invertBAYB);
                ImGui::Separator();
                if (ImGui::MenuItem("Configure Keyboard..."))
                    keyboardDialog.open(invertBAYB ? std::array<int, 10>{0, 1, 2, 3, 6, 4, 7, 5, 8, 9}
                                                   : std::array<int, 10>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
                                        true);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug"))
            {
                if (ImGui::MenuItem("Disassembler", nullptr, isDisassemblerOpen()))
                {
                    if (isDisassemblerOpen())
                        debugWindowManager.closeDisassembler();
                    else
                        openDisassembler();
                }
                if (ImGui::MenuItem("RAM Viewer", nullptr, isRamViewerOpen()))
                {
                    if (isRamViewerOpen())
                        debugWindowManager.closeRamViewer();
                    else
                        openRamViewer();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Tile Viewer", nullptr, isTileViewerOpen()))
                {
                    if (isTileViewerOpen())
                        debugWindowManager.closeTileViewer();
                    else
                        openTileViewer();
                }
                if (ImGui::MenuItem("VRAM Viewer", nullptr, isVramViewerOpen()))
                {
                    if (isVramViewerOpen())
                        debugWindowManager.closeVramViewer();
                    else
                        openVramViewer();
                }
                if (ImGui::MenuItem("Palette Viewer", nullptr, isPaletteViewerOpen()))
                {
                    if (isPaletteViewerOpen())
                        debugWindowManager.closePaletteViewer();
                    else
                        openPaletteViewer();
                }
                if (ImGui::MenuItem("OAM Viewer", nullptr, isOamViewerOpen()))
                {
                    if (isOamViewerOpen())
                        debugWindowManager.closeOamViewer();
                    else
                        openOamViewer();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Cheats"))
            {
                const auto state = cheatMenuCallback ? cheatMenuCallback() : CheatMenuState{};
                const bool available = cartLoaded && state.available;
                const bool enabled = available && state.enabled;
                if (ImGui::MenuItem("Enable Cheats", nullptr, enabled, available && static_cast<bool>(cheatsEnabledCallback)))
                {
                    cheatsEnabledCallback(!enabled);
                }
                if (ImGui::BeginMenu("Cheat List", enabled))
                {
                    const auto &entries = state.entries;
                    for (size_t i = 0; i < entries.size(); ++i)
                    {
                        const auto &entry = entries[i];
                        ImGui::PushID(static_cast<int>(i));
                        // Descriptions may contain ImGui's hidden-label separator.
                        const auto label = entry.description + "###cheat";
                        if (ImGui::MenuItem(label.c_str(), nullptr, entry.enabled, entry.valid && static_cast<bool>(cheatEntryEnabledCallback)))
                        {
                            cheatEntryEnabledCallback(i, !entry.enabled);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Hacks"))
            {
                if (ImGui::MenuItem("Enable Fast-Forward", "Tab / R2 (hold)", fastForwardEnabled))
                    setFastForward(!fastForwardEnabled);
                if (ImGui::MenuItem("Enable Rewind", "Caps Lock / L2 (hold)", rewindEnabled))
                    setRewind(!rewindEnabled);
                if (ImGui::BeginMenu("Rewind Level", rewindEnabled))
                {
                    const char *levels[] = {"Light", "Normal", "Precise"};
                    for (int level = 0; level < 3; ++level)
                        if (ImGui::MenuItem(levels[level], nullptr, rewindPrecisionLevel == level))
                            setRewindPrecision(level);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Enable Unlimited Sprites", nullptr, unlimitedSprites))
                    setUnlimitedSprites(!unlimitedSprites);
                ImGui::Separator();
                if (ImGui::MenuItem("Enable CPU Overclock", nullptr, cpuOverclockEnabled))
                    setCPUOverclock(!cpuOverclockEnabled);
                ImGui::BeginDisabled(!cpuOverclockEnabled);
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::SliderFloat("CPU Multiplier", &cpuMultiplier, 1.0f, 2.0f, "%.2fx") && cpuOverclockCallback)
                    cpuOverclockCallback(cpuOverclockEnabled, cpuMultiplier);
                ImGui::EndDisabled();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("About"))
            {
                openAbout = ImGui::MenuItem("About R2NES");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (openAbout)
            ImGui::OpenPopup("About R2NES");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        bool aboutVisible = true;
        if (ImGui::BeginPopupModal("About R2NES", &aboutVisible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextUnformatted("Software livre e aberto para a comunidade.");
            ImGui::TextUnformatted("Ver licen\xC3\xA7" "a de uso.");
            ImGui::Spacing();
            ImGui::TextUnformatted("Vers\xC3\xA3" "o: " R2_APP_VERSION " - Data: 13/11/2026");
            ImGui::EndPopup();
        }

        renderFileDialog();
        keyboardDialog.draw([this](const R2Keyboard::Mapping &mapping)
                            {
            if (!keyboardConfig.save(mapping)) return false;
            if (keyboardMappingCallback) keyboardMappingCallback(mapping);
            return true; });
        updateMenuPause(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId));
    }

    // Atualiza o estado do menu de pausa com base na abertura do menu.
    void Window::updateMenuPause(bool menuOpen)
    {
        if (!menuOpen)
        {
            menuPauseSuspendedUntilClose = false;
            if (menuPauseActive)
                releaseMenuPause(false);
            return;
        }

        if (menuPauseSuspendedUntilClose)
            return;

        if (!menuPauseActive)
        {
            menuPauseActive = true;
            pauseState.setMenu(true);
            publishPause();
        }
    }

    // Libera o estado de pausa do menu, possivelmente suspendendo até que o menu seja fechado.
    void Window::releaseMenuPause(bool suspendUntilClosed)
    {
        if (menuPauseActive)
        {
            menuPauseActive = false;
            pauseState.setMenu(false);
            publishPause();
        }

        if (suspendUntilClosed)
            menuPauseSuspendedUntilClose = true;
    }

    // Abre o desassemblador ao lado da janela principal.
    void Window::openDisassembler()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        debugWindowManager.openDisassembler(x, y, w);
    }

    // Repassa o estado atual da CPU ao visualizador de desassembly.
    void Window::updateDisassembler(uint16_t pc, const std::map<uint16_t, std::string> &disassembly,
                                    bool &stepByStep, bool &stepRequested, uint8_t a, uint8_t x, uint8_t y, uint8_t stkp, uint8_t status)
    {
        debugWindowManager.updateDisassembler(pc, disassembly, stepByStep, stepRequested, a, x, y, stkp, status);
    }

    // Abre o visualizador das pattern tables junto à janela principal.
    void Window::openTileViewer()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        debugWindowManager.openTileViewer(x, y, w);
    }

    // Abre o visualizador de paleta junto à janela principal.
    void Window::openPaletteViewer()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        debugWindowManager.openPaletteViewer(x, y, w);
    }

    // Repassa as duas pattern tables ao Tile Viewer.
    void Window::updateTileViewer(const uint32_t *pixels0, const uint32_t *pixels1)
    {
        debugWindowManager.updateTileViewer(pixels0, pixels1);
    }

    // Repassa a paleta da PPU e a paleta de cores selecionada ao visualizador.
    void Window::updatePaletteViewer(const std::array<uint8_t, 32> &paletteTable, const uint32_t *systemPalette)
    {
        debugWindowManager.updatePaletteViewer(paletteTable, systemPalette);
    }

    // Abre o visualizador de RAM junto à janela principal.
    void Window::openRamViewer()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        debugWindowManager.openRamViewer(x, y, w);
    }

    // Atualiza o RAM Viewer somente enquanto sua janela existir.
    void Window::updateRamViewer(const MemoryAccess &memory)
    {
        debugWindowManager.updateRamViewer(memory);
    }

    // Abre o visualizador de VRAM junto à janela principal.
    void Window::openVramViewer()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        debugWindowManager.openVramViewer(x, y, w);
    }

    // Atualiza o VRAM Viewer somente enquanto sua janela existir.
    void Window::updateVramViewer(const MemoryAccess &memory)
    {
        debugWindowManager.updateVramViewer(memory);
    }

    // Abre o visualizador da memória de atributos de sprites.
    void Window::openOamViewer()
    {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);

        debugWindowManager.openOamViewer(x, y, w);
    }

    // Atualiza o OAM Viewer somente enquanto sua janela existir.
    void Window::updateOamViewer(const std::array<uint8_t, 256> &oam)
    {
        debugWindowManager.updateOamViewer(oam);
    }

    // Solicita o descarregamento da ROM e fecha ferramentas dependentes do cartucho.
    void Window::unload()
    {
        unloadRequested = true;
        setPaused(false);
        this->uncheckAllDebugMenuItems();
        this->uncheckZapperMenu();
    }

    // Solicita reset e fecha ferramentas que exibem estado potencialmente inválido.
    void Window::reset()
    {
        resetRequested = true;
        setPaused(false);
        this->uncheckAllDebugMenuItems();
    }

    // Desativa a Zapper na interface após remover ou trocar a ROM.
    void Window::uncheckZapperMenu()
    {
        std::cout << "uncheck zapper menu";
        this->useZapper = false;
    }

    // Fecha todos os visualizadores e remove suas marcações do menu de depuração.
    void Window::uncheckAllDebugMenuItems()
    {
        debugWindowManager.closeAll();
    }

    // Atualiza a preferência de fast-forward e a propaga à Engine.
    void Window::setFastForward(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (fastForwardEnabled == enabled)
            return;

        fastForwardEnabled = enabled;

        // Notificar a Engine sobre a mudança
        if (ffCallback)
            ffCallback(fastForwardEnabled);

        std::cout << "Window: Fast Forward " << (fastForwardEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a preferência de rewind e a propaga à Engine.
    void Window::setRewind(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (rewindEnabled == enabled)
            return;

        rewindEnabled = enabled;

        // Notificar a Engine sobre a mudança
        if (rewindCallback)
            rewindCallback(rewindEnabled);

        std::cout << "Window: Rewind " << (rewindEnabled ? "Enabled" : "Disabled") << std::endl;

        // Recreate menu so dependent items (Rewind Level popup) update enabled state
        createMenu();
    }

    void Window::setRewindPrecision(int level)
    {
        if (level < 0)
            level = 0;
        if (level > 2)
            level = 2;

        if (rewindPrecisionLevel == level)
            return;

        rewindPrecisionLevel = level;
        std::cout << "Window: Rewind Precision set to " << rewindPrecisionLevel << std::endl;

        // Recreate menu to update checked radio item
        createMenu();
    }

    // Atualiza o som mestre e notifica o subsistema de áudio.
    void Window::setSound(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (soundEnabled == enabled)
            return;

        soundEnabled = enabled;

        // Notificar a Engine sobre a mudança para ajustar o áudio
        if (soundCallback)
            soundCallback(soundEnabled);

        std::cout << "Window: Sound " << (soundEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a ativação do primeiro canal pulse da APU.
    void Window::setPulse1(bool enabled)
    {
        if (pulse1Enabled == enabled)
            return;

        pulse1Enabled = enabled;

        if (pulse1Callback)
            pulse1Callback(pulse1Enabled);
        std::cout << "Sound: Pulse1 Channel " << (pulse1Enabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a ativação do segundo canal pulse da APU.
    void Window::setPulse2(bool enabled)
    {
        if (pulse2Enabled == enabled)
            return;

        pulse2Enabled = enabled;

        if (pulse2Callback)
            pulse2Callback(pulse2Enabled);

        std::cout << "Sound: Pulse2 Channel " << (pulse2Enabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a ativação do canal triangle da APU.
    void Window::setTriangle(bool enabled)
    {
        if (triangleEnabled == enabled)
            return;

        triangleEnabled = enabled;

        if (triangleCallback)
            triangleCallback(triangleEnabled);

        std::cout << "Sound: Triangle Channel " << (triangleEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a ativação do canal noise da APU.
    void Window::setNoise(bool enabled)
    {
        if (noiseEnabled == enabled)
            return;

        noiseEnabled = enabled;

        if (noiseCallback)
            noiseCallback(noiseEnabled);

        std::cout << "Sound: Noise Channel " << (noiseEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a ativação do canal DMC da APU.
    void Window::setDMC(bool enabled)
    {
        if (dmcEnabled == enabled)
            return;

        dmcEnabled = enabled;

        if (dmcCallback)
            dmcCallback(dmcEnabled);

        std::cout << "Sound: DMC Channel " << (dmcEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza o hack de sprites ilimitados e o propaga à Engine.
    void Window::setUnlimitedSprites(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (unlimitedSprites == enabled)
            return;

        unlimitedSprites = enabled;

        // Notificar a Engine sobre a mudança para ajustar a lógica de renderização
        if (unlimitedSpritesCallback)
            unlimitedSpritesCallback(unlimitedSprites);

        std::cout << "Window: Unlimited Sprites " << (unlimitedSprites ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza a inversão dos botões e informa o InputManager pela Engine.
    void Window::setInvertBAYB(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (invertBAYB == enabled)
            return;

        invertBAYB = enabled;

        // Notificar a Engine sobre a mudança para ajustar a lógica de renderização
        if (invertBAYBCallback)
            invertBAYBCallback(invertBAYB);

        std::cout << "Window: Inverted BA/YB " << (invertBAYB ? "Enabled" : "Disabled") << std::endl;
    }

    // Atualiza o uso da Zapper e informa o InputManager pela Engine.
    void Window::setUseZapper(bool enabled)
    {
        if (useZapper == enabled)
            return;

        useZapper = enabled;

        if (useZapperCallback)
            useZapperCallback(useZapper);
        std::cout << "Input: Use Zapper " << (useZapper ? "Enabled" : "Disabled") << std::endl;
    }

    // Mantém a preferência do usuário mesmo quando o fast-forward suspende VSync.
    void Window::setVSync(bool enabled)
    {
        // Se não houve mudança e o renderer já existe, não fazemos nada
        if (vsyncEnabled == enabled && renderer != nullptr)
            return;

        vsyncEnabled = enabled;
        applyVSync(vsyncEnabled && !vsyncSuppressed);

        if (vsyncCallback)
            vsyncCallback(vsyncEnabled);

        std::cout << "Window: VSync " << (vsyncEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    void Window::setVSyncSuppressed(bool suppressed)
    {
        if (vsyncSuppressed == suppressed)
            return;

        vsyncSuppressed = suppressed;
        applyVSync(vsyncEnabled && !vsyncSuppressed);
    }

    // A troca direta evita destruir texturas e o backend ImGui a cada atalho.
    void Window::applyVSync(bool enabled)
    {
        if (renderer && rendererVsyncEnabled == enabled)
            return;

        if (renderer && SDL_SetRenderVSync(renderer, enabled ? 1 : 0))
        {
            rendererVsyncEnabled = enabled;
            return;
        }

        // SDL3 configura VSync pela API, sem flags de criação do renderer.
        // Se o driver não o oferece, a Engine usa sua cadência por relógio.
        if (renderer)
        {
            std::cerr << "Window: Could not set VSync: " << SDL_GetError() << std::endl;
            if (enabled && !vsyncSuppressed)
            {
                vsyncEnabled = false;
                if (vsyncCallback)
                    vsyncCallback(false);
            }
        }
    }

    void Window::restoreFullscreenWindow()
    {
        if (!fullscreenResolution.leave(window))
            return;
        if (displayTransitionCallback)
            displayTransitionCallback();
        SDL_SetWindowBordered(window, true);
        SDL_SetWindowPosition(window, lastWindowedX, lastWindowedY);
        SDL_SetWindowSize(window, lastWindowedW, lastWindowedH);
        createMenu();
        currentDisplayMode = DisplayMode::WINDOWED;
    }

    // Ajusta o tamanho visível conforme escala e recorte de overscan atuais.
    void Window::windowResize(int times)
    {
        // Exit fullscreen mode if currently in one
        if (currentDisplayMode != DisplayMode::WINDOWED)
        {
            fullscreenResolution.leave(window);
            if (displayTransitionCallback)
                displayTransitionCallback();
            SDL_SetWindowBordered(window, true); // Restore border
            createMenu();                        // Restaura o menu nativo
        }

        int currentW = width;
        int currentH = height;
        if (fullCropOverscan)
        {
            currentW -= 16;
            currentH -= 16;
        }
        else if (cropOverscan)
        {
            currentH -= 16;
        }
        SDL_SetWindowSize(window, currentW * times * scale, currentH * times * scale);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        // Update last known windowed size
        SDL_GetWindowSize(window, &lastWindowedW, &lastWindowedH);
        SDL_GetWindowPosition(window, &lastWindowedX, &lastWindowedY);
        currentDisplayMode = DisplayMode::WINDOWED;
        this->currentWindowX = times;
    }

    // Salva o modo janela, remove o menu nativo e entra em fullscreen sem bordas.
    void Window::setWindowBorderlessFullscreen(DisplayMode dm)
    {
        // Save current windowed state before going fullscreen
        if (currentDisplayMode == DisplayMode::WINDOWED)
        {
            SDL_GetWindowPosition(window, &lastWindowedX, &lastWindowedY);
            SDL_GetWindowSize(window, &lastWindowedW, &lastWindowedH);
        }

        if (fullscreenResolution.enter(window))
        {
            currentDisplayMode = dm;
            if (displayTransitionCallback)
                displayTransitionCallback();
        }
    }

    // Entra em fullscreen preenchendo toda a área disponível.
    void Window::windowBorderlessFullscreenStretch()
    {
        this->setWindowBorderlessFullscreen(DisplayMode::FULLSCREEN_STRETCH);
    }

    // Entra em fullscreen preservando a proporção de pixels do NES.
    void Window::windowBorderlessFullscreen()
    {
        this->setWindowBorderlessFullscreen(DisplayMode::FULLSCREEN_ASPECT_8_7);
    }

    // Altera a interpolação na própria textura, sem recriar o renderer.
    void Window::setBilinearFiltering(bool enabled)
    {
        if (!texture || bilinearFiltering == enabled)
            return;

        if (!SDL_SetTextureScaleMode(texture, enabled ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST))
        {
            std::cerr << "Window: Could not set texture filtering: " << SDL_GetError() << std::endl;
            return;
        }

        bilinearFiltering = enabled;
    }

    // Atualiza a ativação do filtro de scanlines.
    void Window::setScanlines(bool enabled)
    {
        if (scanlines == enabled)
            return;

        scanlines = enabled;

        std::cout << "Window: Scanlines " << (scanlines ? "Enabled" : "Disabled") << std::endl;

        // Recreate menu so dependent items (Scanlines) are updated correctly.
        createMenu();
    }

    // Atualiza o hack de overclock e o propaga à Engine.
    void Window::setCPUOverclock(bool enabled)
    {
        // Se não houve mudança, não fazemos nada
        if (cpuOverclockEnabled == enabled)
            return;

        cpuOverclockEnabled = enabled;

        // Notificar a Engine sobre a mudança
        if (cpuOverclockCallback)
            cpuOverclockCallback(cpuOverclockEnabled, cpuMultiplier);

        std::cout << "Hacks: CPU Overclock " << (cpuOverclockEnabled ? "Enabled" : "Disabled") << std::endl;
    }

    // Compõe o quadro da PPU, aplica pós-processamento e o apresenta pela SDL.
    void Window::render(const uint32_t *pixels, float fps)
    {
        if (closed || !renderer || !texture)
            return;

        // Atualiza o título da janela com o FPS
        static float lastFps = -1.0f;
        static bool lastPausedState = false;
        static bool lastOverclockState = false;
        static bool lastCartLoaded = false;
        static std::string lastRomFile;

        if (fps != lastFps || paused != lastPausedState || cpuOverclockEnabled != lastOverclockState ||
            cartLoaded != lastCartLoaded || romFile != lastRomFile)
        {
            char titleBuffer[512];
            std::snprintf(titleBuffer, sizeof(titleBuffer), "%s | FPS: %.2f", title.c_str(),
                          paused ? 0.0f : fps);
            std::string displayTitle = titleBuffer;
            if (cartLoaded)
                displayTitle += " | " + romFile;
            if (cpuOverclockEnabled)
            {
                displayTitle += " | CPU OVERCLOCK ENABLED! GAMES CAN CRASH!";
            }
            SDL_SetWindowTitle(window, displayTitle.c_str());
            lastFps = fps;
            lastPausedState = paused;
            lastOverclockState = cpuOverclockEnabled;
            lastCartLoaded = cartLoaded;
            lastRomFile = romFile;
        }

        renderVideo(pixels);

        if (imguiContext)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            renderMenu();
            if (closed)
                return;

            if (paused && currentDisplayMode != DisplayMode::WINDOWED)
            {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::Begin("PauseOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
                ImGui::SetWindowFontScale(4.0f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), "PAUSED");
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        }

        SDL_RenderPresent(renderer);
    }
}
