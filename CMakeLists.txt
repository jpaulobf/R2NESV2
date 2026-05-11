cmake_minimum_required(VERSION 3.14)

# Força o uso do compilador real e desativa o launcher do ccache que está quebrando no Windows
set(CMAKE_C_COMPILER gcc CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER g++ CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)

# Resolve problemas do ccache no Windows com caminhos longos e arquivos .rsp
# Deve vir ANTES de project() para garantir que afete todas as dependências
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF CACHE BOOL "" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "" FORCE)

# Nome do projeto e versão
project(R2NESV2 VERSION 1.0 LANGUAGES CXX)

# Padrão do C++ (conforme seu c_cpp_properties.json)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define o diretório de saída para a pasta /bin na raiz (como você pediu)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Procura automaticamente por todos os arquivos .cpp dentro de src e subpastas
# Isso resolve seu problema de "packages/pastas" automaticamente!
file(GLOB_RECURSE SOURCES "src/*.cpp")

include(FetchContent)

# Declara a dependência do SDL2 buscando direto do GitHub oficial
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-2.30.3 # Versão estável do SDL2
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.6
)

FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        v1.3.1
)

# Configurações Globais para Dependências
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Força tudo como estático (.a)

# Silencia os erros de tipos de ponteiro da libzip no MinGW/GCC 13
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-incompatible-pointer-types")

# Baixa e torna o SDL2 disponível para o projeto
FetchContent_MakeAvailable(SDL2)
FetchContent_MakeAvailable(imgui)

# Precisamos configurar a zlib primeiro
# Desativamos exemplos da zlib para acelerar o build
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

# Adiciona os arquivos de backend do ImGui (SDL2 + SDL_Renderer2)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

# Cria o executável
add_executable(R2NESV2 ${SOURCES} ${IMGUI_SOURCES} resources/resource.rc)

# Inclui a pasta src para permitir #include "Core/Bus.h"
# Usar target_include_directories é a prática recomendada em C++ moderno
target_include_directories(R2NESV2 PRIVATE 
    src 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${zlib_SOURCE_DIR}
    ${zlib_BINARY_DIR}
)

# Linka o SDL2 e zlib ao executável
target_link_libraries(R2NESV2 PRIVATE 
    SDL2::SDL2main 
    SDL2::SDL2-static 
    zlibstatic
)

# Copia a pasta resources para o diretório de saída do executável após o build
add_custom_command(TARGET R2NESV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:R2NESV2>/resources"
)