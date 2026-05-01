cmake_minimum_required(VERSION 3.10)

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

# Cria o executável
add_executable(R2NESV2 ${SOURCES})

# Inclui a pasta src para permitir #include "Core/Bus.h"
# Usar target_include_directories é a prática recomendada em C++ moderno
target_include_directories(R2NESV2 PRIVATE src)