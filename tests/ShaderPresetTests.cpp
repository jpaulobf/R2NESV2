#include "Window/ShaderPreset.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace R2NES::Core;

static void check(bool value, const char *message)
{
      if (!value)
            throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
      try
      {
            check(argc == 3, "Expected preset and test directory");
            std::vector<ShaderPass> passes;
            std::vector<ShaderPassSize> sizes;
            std::string error;
            if (!loadShaderPreset(argv[1], passes, error))
                  throw std::runtime_error(error);
            check(passes.size() == 4, "Wrong number of passes");
            check(passes[0].floatingPoint && passes[0].frameCountMod == 2 && !passes[0].linear &&
                      !passes[1].linear && !passes[2].linear && passes[3].linear,
                  "Wrong pass properties");
            if (!planShaderPasses(passes, {256, 240}, {768, 720}, sizes, error))
                  throw std::runtime_error(error);
            check(sizes.size() == 4 && sizes[0].width == 1024 && sizes[0].height == 240 &&
                      sizes[1].width == 512 && sizes[1].height == 240 &&
                      sizes[2].width == 512 && sizes[2].height == 720 &&
                      sizes[3].width == 768 && sizes[3].height == 720,
                  "Wrong pass dimensions");
            if (!planShaderPasses(passes, {240, 224}, {1200, 900}, sizes, error))
                  throw std::runtime_error(error);
            check(sizes[0].height == 224 && sizes[1].height == 224 && sizes[2].height == 900 &&
                      sizes[3].width == 1200,
                  "Crop or viewport resize ignored");
            const auto dir = std::filesystem::path(argv[2]);
            std::filesystem::create_directories(dir);
            const auto preset = dir / "missing.cgp";
            std::filesystem::remove(dir / "missing.cg");
            std::filesystem::remove(dir / "absent.inc");
            {
                  std::ofstream out(preset);
                  out << "shaders = 1\nshader0 = missing.cg\n";
            }
            check(!loadShaderPreset(preset, passes, error) && error.find("missing.cg") != std::string::npos,
                  "Missing shader was accepted");
            {
                  std::ofstream out(dir / "missing.cg");
                  out << "#include \"absent.inc\"\n";
            }
            check(!loadShaderPreset(preset, passes, error) && error.find("absent.inc") != std::string::npos,
                  "Missing include was accepted");
            {
                  std::ofstream out(dir / "missing.cg");
                  out << "// shader fixture\n";
            }
            {
                  std::ofstream out(preset);
                  out << "shaders = 1\nshader0 = missing.cg\ntextures = LUT\nLUT = absent.png\nLUT_linear = false\n";
            }
            std::filesystem::remove(dir / "absent.png");
            check(!loadShaderPreset(preset, passes, error) && error.find("absent.png") != std::string::npos,
                  "Missing lookup texture was accepted");
            check(!planShaderPasses(passes, {256, 240}, {0, 720}, sizes, error), "Invalid viewport was accepted");
            std::cout << "Shader preset tests passed\n";
            return 0;
      }
      catch (const std::exception &e)
      {
            std::cerr << e.what() << '\n';
            return 1;
      }
}