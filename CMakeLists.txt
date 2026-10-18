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

# Procura automaticamente por todos os arquivos .cpp dentro de src e subpastas.
# CONFIGURE_DEPENDS reconfigura o projeto quando novos arquivos são adicionados.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

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
set(PLATFORM_SOURCES)
if(WIN32)
  list(APPEND PLATFORM_SOURCES resources/resource.rc)
endif()

add_executable(R2NESV2 ${SOURCES} ${IMGUI_SOURCES} ${PLATFORM_SOURCES})

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

include(CTest)
if(BUILD_TESTING)
  file(GLOB MAPPER_SOURCES CONFIGURE_DEPENDS "src/Core/Cartridge/Mappers/*.cpp")
  add_executable(mapper002-tests tests/Mapper002Tests.cpp
    src/Core/Cartridge/Cartridge.cpp src/Core/Cartridge/BatterySave.cpp src/Core/Memory/VRAM/VRAM.cpp ${MAPPER_SOURCES})
  target_include_directories(mapper002-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper002-tests PRIVATE zlibstatic)
  target_sources(mapper002-tests PRIVATE src/Core/Cartridge/UNIF.cpp)
  add_test(NAME mapper002 COMMAND mapper002-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper002-test-roms")
  file(GLOB_RECURSE CORE_TEST_SOURCES CONFIGURE_DEPENDS "src/Core/*.cpp")
  add_executable(battery-tests tests/BatteryMapperTests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(battery-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(battery-tests PRIVATE zlibstatic)
  add_test(NAME battery COMMAND battery-tests "${CMAKE_CURRENT_BINARY_DIR}/battery-test-roms")
  add_executable(mapper005-tests tests/Mapper005Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper005-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper005-tests PRIVATE zlibstatic)
  add_test(NAME mapper005 COMMAND mapper005-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper005-test-roms")
  add_executable(mapper007-tests tests/Mapper007Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper007-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper007-tests PRIVATE zlibstatic)
  add_test(NAME mapper007 COMMAND mapper007-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper007-test-roms")
  add_executable(mapper009-tests tests/Mapper009Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper009-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper009-tests PRIVATE zlibstatic)
  add_test(NAME mapper009 COMMAND mapper009-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper009-test-roms")
  add_executable(mapper011-tests tests/Mapper011Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper011-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper011-tests PRIVATE zlibstatic)
  add_test(NAME mapper011 COMMAND mapper011-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper011-test-roms")
  add_executable(sachen-tests tests/SachenTests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(sachen-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(sachen-tests PRIVATE zlibstatic)
  add_test(NAME sachen COMMAND sachen-tests "${CMAKE_CURRENT_BINARY_DIR}/sachen-test-roms")
  add_executable(mapper004-tests tests/Mapper004Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper004-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper004-tests PRIVATE zlibstatic)
  add_test(NAME mapper004 COMMAND mapper004-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper004-test-roms")
  foreach(MMC3_ROM IN ITEMS 1-clocking 2-details 3-A12_clocking 4-scanline_timing 5-MMC3 6-MMC3_alt 6-MMC6)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nestest/blarg/${MMC3_ROM}.nes")
      add_test(NAME mapper004-rom-${MMC3_ROM} COMMAND mapper004-tests
        "${CMAKE_CURRENT_BINARY_DIR}/mapper004-${MMC3_ROM}"
        "${CMAKE_CURRENT_SOURCE_DIR}/nestest/blarg/${MMC3_ROM}.nes")
    endif()
  endforeach()
  add_executable(mapper003-tests tests/Mapper003Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper003-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper003-tests PRIVATE zlibstatic)
  add_test(NAME mapper003 COMMAND mapper003-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper003-test-roms")
  add_executable(mapper001-tests tests/Mapper001Tests.cpp ${CORE_TEST_SOURCES})
  target_include_directories(mapper001-tests PRIVATE src ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(mapper001-tests PRIVATE zlibstatic)
  add_test(NAME mapper001 COMMAND mapper001-tests "${CMAKE_CURRENT_BINARY_DIR}/mapper001-test-roms")
endif()
