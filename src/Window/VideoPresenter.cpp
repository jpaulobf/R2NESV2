#include "Window.h"
#include "VideoLayout.h"
#include "NtscShaderPipeline.h"
#include <SDL3/SDL.h>
#include <string>

namespace R2NES::Core
{
    void Window::renderVideo(const uint32_t *pixels)
    {
        const uint32_t *finalPixels = pixels;

        if (scanlines)
        {
            // Mantém um buffer separado para não modificar o framebuffer fornecido pela PPU.
            if (postProcessBuffer.size() != width * height)
                postProcessBuffer.resize(width * height);

            float factor = 1.0f - (scanlinesTransparency / 100.0f);

            for (int y = 0; y < height; ++y)
            {
                bool isScanlineRow = (y % 2 != 0);
                uint32_t rowOffset = y * width;

                for (int x = 0; x < width; ++x)
                {
                    uint32_t index = rowOffset + x;
                    uint32_t pixel = pixels[index];

                    if (isScanlineRow)
                    {
                        // Aplica o fator de escurecimento mantendo o canal Alpha
                        pixel = (pixel & 0xFF000000) |
                                (static_cast<uint32_t>(((pixel >> 16) & 0xFF) * factor) << 16) |
                                (static_cast<uint32_t>(((pixel >> 8) & 0xFF) * factor) << 8) |
                                (static_cast<uint32_t>((pixel & 0xFF) * factor));
                    }
                    postProcessBuffer[index] = pixel;
                }
            }
            finalPixels = postProcessBuffer.data();
        }

        SDL_UpdateTexture(texture, nullptr, finalPixels, width * sizeof(uint32_t));
        SDL_RenderClear(renderer);

        // Define a área de origem da textura (Source Rect)
        // Se o Crop estiver ativado, pulamos os primeiros 8 scanlines e reduzimos a altura em 16 (8 topo + 8 base)
        SDL_FRect src_rect = {0, 0, static_cast<float>(width), static_cast<float>(height)};
        if (fullCropOverscan)
        {
            src_rect.x = 8;
            src_rect.y = 8;
            src_rect.w = width - 16;
            src_rect.h = height - 16;
        }
        else if (cropOverscan)
        {
            src_rect.y = 8;
            src_rect.h = height - 16;
        }

        SDL_FRect dest_rect;
        if (currentDisplayMode == DisplayMode::FULLSCREEN_ASPECT_8_7)
        {
            // Calcula uma área centralizada que preserva a proporção escolhida.
            int current_window_w, current_window_h;
            SDL_GetRenderOutputSize(renderer, &current_window_w, &current_window_h);

            // Se cortamos 16 pixels de altura (8 topo + 8 base), para manter o mesmo "look" dos pixels
            // (Pixel Aspect Ratio), precisamos ajustar a proporção da tela (Display Aspect Ratio).
            // 8:7 original assume 240 linhas. Para 224 linhas, a nova proporção é 60:49.
            double target_aspect_ratio;
            if (fullCropOverscan)
            {
                // 240x224 -> 60:49
                target_aspect_ratio = 60.0 / 49.0;
            }
            else if (cropOverscan)
            {
                // 256x224 -> 8:7
                target_aspect_ratio = 8.0 / 7.0;
            }
            else
            {
                target_aspect_ratio = 8.0 / 7.0;
            }

            const auto layout = R2Window::videoLayoutInteger(current_window_w, current_window_h, target_aspect_ratio, false);
            dest_rect = {layout.x, layout.y, layout.width, layout.height};
        }
        else
        {
            int outputWidth = 0, outputHeight = 0;
            SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight);
            dest_rect = {0, 0, static_cast<float>(outputWidth), static_cast<float>(outputHeight)};
        }

        bool renderedShader = false;
        NtscShaderPipeline *activeShader = shader == ShaderType::SCANLINES ? ntscShader.get()
                                           : shader == ShaderType::HQ2     ? hq2Shader.get()
                                           : shader == ShaderType::SCALEFX ? scaleFxShader.get()
                                           : shader == ShaderType::CRT     ? crtShader.get()
                                           : shader == ShaderType::CRT3D   ? crt3dShader.get()
                                                                           : nullptr;
        if (activeShader && dest_rect.w >= 1 && dest_rect.h >= 1)
        {
            const int outWidth = static_cast<int>(dest_rect.w), outHeight = static_cast<int>(dest_rect.h);
            const int srcX = static_cast<int>(src_rect.x), srcY = static_cast<int>(src_rect.y);
            std::string error;
            if (activeShader->render(finalPixels + srcY * width + srcX, width,
                                     {static_cast<int>(src_rect.w), static_cast<int>(src_rect.h)},
                                     {outWidth, outHeight}, shaderPixels, error))
            {
                if (!shaderTexture || shaderTextureWidth != outWidth || shaderTextureHeight != outHeight)
                {
                    SDL_DestroyTexture(shaderTexture);
                    shaderTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                      SDL_TEXTUREACCESS_STREAMING, outWidth, outHeight);
                    shaderTextureWidth = outWidth;
                    shaderTextureHeight = outHeight;
                    if (shaderTexture)
                    {
                        SDL_SetTextureBlendMode(shaderTexture, SDL_BLENDMODE_NONE);
                        SDL_SetTextureScaleMode(shaderTexture, SDL_SCALEMODE_NEAREST);
                    }
                }
                if (shaderTexture && SDL_UpdateTexture(shaderTexture, nullptr, shaderPixels.data(), outWidth * sizeof(uint32_t)) &&
                    SDL_RenderTexture(renderer, shaderTexture, nullptr, &dest_rect))
                    renderedShader = true;
                else
                    error = SDL_GetError();
            }
            if (!renderedShader)
            {
                const char *effectName = shader == ShaderType::HQ2       ? "HQ2"
                                         : shader == ShaderType::SCALEFX ? "ScaleFX"
                                         : shader == ShaderType::CRT     ? "CRT 3D"
                                         : shader == ShaderType::CRT3D   ? "CRT"
                                                                         : "Gauss Scanlines";
                shader = ShaderType::NONE;
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, effectName, error.c_str(), window);
            }
        }
        if (!renderedShader)
            SDL_RenderTexture(renderer, texture, &src_rect, &dest_rect);
    }
}