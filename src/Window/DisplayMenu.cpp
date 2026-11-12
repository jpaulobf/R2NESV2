#include "Window.h"
#include "FullscreenMenu.h"
#include <imgui.h>
#include <string>
#include <utility>

namespace R2NES::Core
{
    void Window::renderDisplayMenu()
    {
        if (ImGui::BeginMenu("Display"))
        {
            if (ImGui::BeginMenu("Console Region"))
            {
                if (ImGui::MenuItem("NTSC", nullptr, !palRegion) && palRegion)
                {
                    palRegion = false;
                    if (regionCallback)
                        regionCallback(false);
                }
                if (ImGui::MenuItem("PAL", nullptr, palRegion) && !palRegion)
                {
                    palRegion = true;
                    if (regionCallback)
                        regionCallback(true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("VSync", nullptr, vsyncEnabled))
                setVSync(!vsyncEnabled);
            ImGui::Separator();
            for (int scaleValue = 1; scaleValue <= 4; ++scaleValue)
            {
                const std::string label = std::to_string(scaleValue) + "x";
                if (ImGui::MenuItem(label.c_str(), nullptr, currentWindowX == scaleValue))
                    windowResize(scaleValue);
            }
            ImGui::Separator();
            R2Window::drawFullscreenResolutionMenu(fullscreenResolution, window, "Fullscreen Resolution");
            if (ImGui::MenuItem("Fullscreen Stretched"))
                windowBorderlessFullscreenStretch();
            if (ImGui::MenuItem("Fullscreen", "Ctrl+F ou Alt+Enter"))
                windowBorderlessFullscreen();
            ImGui::Separator();
            if (ImGui::MenuItem("Bilinear Filtering", nullptr, bilinearFiltering))
                setBilinearFiltering(!bilinearFiltering);
            if (ImGui::MenuItem("Scanlines", nullptr, scanlines))
                setScanlines(!scanlines);
            if (ImGui::BeginMenu("Scanlines Level", scanlines))
            {
                for (const int level : {5, 10, 15, 20, 25})
                {
                    const std::string label = std::to_string(level) + "%";
                    if (ImGui::MenuItem(label.c_str(), nullptr, scanlinesTransparency == level))
                        scanlinesTransparency = level;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Top Crop Overscan (8px)", nullptr, cropOverscan))
            {
                cropOverscan = !cropOverscan;
                if (cropOverscan)
                    fullCropOverscan = false;
                if (currentDisplayMode == DisplayMode::WINDOWED)
                    windowResize(currentWindowX);
            }
            if (ImGui::MenuItem("Full Crop Overscan (8px, 8px, 8px, 8px)", nullptr, fullCropOverscan))
            {
                fullCropOverscan = !fullCropOverscan;
                if (fullCropOverscan)
                    cropOverscan = false;
                if (currentDisplayMode == DisplayMode::WINDOWED)
                    windowResize(currentWindowX);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Render Tiles", nullptr, tilesEnabled))
                toggleTiles();
            if (ImGui::MenuItem("Render Sprites", nullptr, spritesEnabled))
                toggleSprites();
            ImGui::Separator();
            if (ImGui::BeginMenu("Palette Preset"))
            {
                const std::pair<PaletteType, const char *> palettes[] = {{PaletteType::DEFAULT, "Default"}, {PaletteType::SMOOTH, "Smooth"}, {PaletteType::NESTOPIA, "Nestopia Emulator"}, {PaletteType::WAVEBEAM, "WaveBeam"}, {PaletteType::NEON, "Neon"}};
                for (const auto &[palette, label] : palettes)
                    if (ImGui::MenuItem(label, nullptr, palettePreset == palette))
                        setPalettePreset(palette);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Shaders"))
            {
                // Os tipos identificam os presets originais: CRT3D é o
                // Hyllian e CRT é o Lottes, cujos nomes no menu foram trocados.
                const std::pair<ShaderType, const char *> shaders[] = {{ShaderType::NONE, "None"}, {ShaderType::SCANLINES, "Gauss Scanlines"}, {ShaderType::HQ2, "HQ2"}, {ShaderType::CRT3D, "CRT"}, {ShaderType::CRT, "CRT 3D"}, {ShaderType::SCALEFX, "ScaleFX"}};
                for (const auto &[shaderType, label] : shaders)
                    if (ImGui::MenuItem(label, nullptr, shader == shaderType))
                        setShader(shaderType);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
    }
}