#include "Window/NtscShaderPipeline.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace R2NES::Core;
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("Expected preset path");
        NtscShaderPipeline pipeline;
        std::string error;
        if (!pipeline.initialize(argv[1], error))
            throw std::runtime_error(error);
        std::vector<uint32_t> input(256 * 240), first, second, resized;
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 256; ++x)
                input[y * 256 + x] = ((x / 8 + y / 8) % 2) ? 0xffff4000 : 0xff0080ff;
        if (!pipeline.render(input.data(), 256, {256, 240}, {512, 480}, first, error))
            throw std::runtime_error(error);
        if (!pipeline.render(input.data(), 256, {256, 240}, {512, 480}, second, error))
            throw std::runtime_error(error);
        if (first.size() != 512 * 480 || first == second ||
            std::all_of(first.begin(), first.end(), [&](uint32_t p)
                        { return p == first.front(); }))
            throw std::runtime_error("Output is uniform, wrong size, or frame modulation is absent");
        if (!pipeline.render(input.data() + 8 * 256 + 8, 256, {240, 224}, {360, 336}, resized, error))
            throw std::runtime_error(error);
        if (resized.size() != 360 * 336)
            throw std::runtime_error("Resize failed");
        std::cout << "NTSC GPU passes passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}