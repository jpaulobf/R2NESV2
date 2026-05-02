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

# Baixa e torna o SDL2 disponível para o projeto
FetchContent_MakeAvailable(SDL2)

# Cria o executável
add_executable(R2NESV2 ${SOURCES})

# Inclui a pasta src para permitir #include "Core/Bus.h"
# Usar target_include_directories é a prática recomendada em C++ moderno
target_include_directories(R2NESV2 PRIVATE src)

# Linka o SDL2 ao executável e inclui seus diretórios de cabeçalho
target_link_libraries(R2NESV2 PRIVATE SDL2::SDL2-static)