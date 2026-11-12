cmake_minimum_required(VERSION 3.16)

# Avoid broken ccache/response-file combinations in the existing MinGW setup.
set(CMAKE_C_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF CACHE BOOL "" FORCE)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS OFF CACHE BOOL "" FORCE)

project(R2NESV2 VERSION 1.3.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(R2NES_BUILD_GUI "Build the SDL3 / Dear ImGui application and GUI tests" ON)

if(R2NES_BUILD_GUI)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
else()
  # Keep headless builds from overwriting executables from the GUI build.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1)
FetchContent_MakeAvailable(zlib)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/Core/*.cpp")
add_library(r2nes_core STATIC ${CORE_SOURCES})
target_include_directories(r2nes_core PUBLIC src PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
target_link_libraries(r2nes_core PRIVATE zlibstatic)
add_library(r2nes_host STATIC src/System/RomReader.cpp src/System/BatterySave.cpp src/System/BatterySession.cpp)
target_include_directories(r2nes_host PUBLIC src)
target_link_libraries(r2nes_host PUBLIC r2nes_core)


if(R2NES_BUILD_GUI)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_GPU ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.93.0)
  FetchContent_MakeAvailable(SDL3 imgui)

  set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
  file(GLOB APP_SOURCES CONFIGURE_DEPENDS "src/System/*.cpp" "src/Util/*.cpp" "src/Window/*.cpp")
  list(REMOVE_ITEM APP_SOURCES
    "${CMAKE_SOURCE_DIR}/src/System/RomReader.cpp"
    "${CMAKE_SOURCE_DIR}/src/System/BatterySave.cpp"
    "${CMAKE_SOURCE_DIR}/src/System/BatterySession.cpp")
  set(PLATFORM_SOURCES)
  if(WIN32)
    list(APPEND PLATFORM_SOURCES resources/resource.rc)
  endif()
  add_executable(R2NESV2 src/Emulator.cpp ${APP_SOURCES} ${IMGUI_SOURCES} ${PLATFORM_SOURCES})
  target_include_directories(R2NESV2 PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(R2NESV2 PRIVATE r2nes_host SDL3::SDL3-static zlibstatic)
  if(WIN32)
    target_link_libraries(R2NESV2 PRIVATE d3d11 d3dcompiler ole32 windowscodecs uuid)
  endif()
  target_include_directories(R2NESV2 PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  add_custom_command(TARGET R2NESV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    "-DRESOURCE_SOURCE=${CMAKE_SOURCE_DIR}/resources"
    "-DRESOURCE_DESTINATION=$<TARGET_FILE_DIR:R2NESV2>/resources"
    -P "${CMAKE_SOURCE_DIR}/cmake/CopyResources.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:R2NESV2>/shaders")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(video_layout_tests tests/VideoLayoutTests.cpp)
  target_include_directories(video_layout_tests PRIVATE src)
  add_test(NAME video_layout_tests COMMAND video_layout_tests)
  add_executable(pause-state-tests tests/PauseStateTests.cpp)
  target_include_directories(pause-state-tests PRIVATE src)
  add_test(NAME pause-state COMMAND pause-state-tests)

  add_executable(snapshot-buffer-tests tests/SnapshotBufferTests.cpp)
  target_include_directories(snapshot-buffer-tests PRIVATE src)
  add_test(NAME snapshot-buffer COMMAND snapshot-buffer-tests)

  add_executable(shader-preset-tests tests/ShaderPresetTests.cpp src/Window/ShaderPreset.cpp)
  target_include_directories(shader-preset-tests PRIVATE src)
  add_test(NAME shader-preset COMMAND shader-preset-tests
    "${CMAKE_SOURCE_DIR}/shaders/ntsc/ntsc-256px-gauss-scanline.cgp"
    "${CMAKE_CURRENT_BINARY_DIR}/shader-preset-test-data")

  if(WIN32)
    add_executable(ntsc-shader-pipeline-tests tests/NtscShaderPipelineTests.cpp
      src/Window/NtscShaderPipeline.cpp src/Window/ShaderPreset.cpp)
    target_include_directories(ntsc-shader-pipeline-tests PRIVATE src)
    target_link_libraries(ntsc-shader-pipeline-tests PRIVATE d3d11 d3dcompiler ole32 windowscodecs uuid)
    add_test(NAME ntsc-shader-pipeline COMMAND ntsc-shader-pipeline-tests
      "${CMAKE_SOURCE_DIR}/shaders/ntsc/ntsc-256px-gauss-scanline.cgp")
    add_executable(shader-effects-tests tests/ShaderEffectsTests.cpp
      src/Window/NtscShaderPipeline.cpp src/Window/ShaderPreset.cpp)
    target_include_directories(shader-effects-tests PRIVATE src)
    target_link_libraries(shader-effects-tests PRIVATE d3d11 d3dcompiler ole32 windowscodecs uuid)
    add_test(NAME hq2-shader-pipeline COMMAND shader-effects-tests
      "${CMAKE_SOURCE_DIR}/shaders/hqx/hq2x-visual.cgp")
    add_test(NAME scalefx-shader-pipeline COMMAND shader-effects-tests
      "${CMAKE_SOURCE_DIR}/shaders/scalefx/scalefx-visual.cgp")
    add_executable(crt-shader-pipeline-tests tests/CrtShaderPipelineTests.cpp
      src/Window/NtscShaderPipeline.cpp src/Window/ShaderPreset.cpp)
    target_include_directories(crt-shader-pipeline-tests PRIVATE src)
    target_link_libraries(crt-shader-pipeline-tests PRIVATE d3d11 d3dcompiler ole32 windowscodecs uuid)
    add_test(NAME crt-lottes-shader-pipeline COMMAND crt-shader-pipeline-tests
      "${CMAKE_SOURCE_DIR}/shaders/crt/crt-lottes.cgp" "${CMAKE_SOURCE_DIR}/shaders/stock.cg"
      "${CMAKE_CURRENT_BINARY_DIR}/crt-lottes-test-data")
    add_test(NAME crt-hyllian-shader-pipeline COMMAND crt-shader-pipeline-tests
      "${CMAKE_SOURCE_DIR}/shaders/crt/crt-hyllian-3d-nes.cgp" "${CMAKE_SOURCE_DIR}/shaders/stock.cg"
      "${CMAKE_CURRENT_BINARY_DIR}/crt-hyllian-test-data")
  endif()

  if(R2NES_BUILD_GUI)
    add_executable(audio-manager-tests tests/AudioManagerTests.cpp src/System/AudioManager.cpp)
    target_include_directories(audio-manager-tests PRIVATE src)
    target_link_libraries(audio-manager-tests PRIVATE SDL3::SDL3-static)
    add_test(NAME audio-manager COMMAND audio-manager-tests)
    set_tests_properties(audio-manager PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" TIMEOUT 10)

    add_executable(event-wait-tests tests/EventWaitTests.cpp)
    target_link_libraries(event-wait-tests PRIVATE SDL3::SDL3-static)
    add_test(NAME event-wait COMMAND event-wait-tests)
    set_tests_properties(event-wait PROPERTIES TIMEOUT 10)

    file(GLOB WINDOW_TEST_SOURCES CONFIGURE_DEPENDS "src/Window/*.cpp")
    add_executable(sdl3-integration-tests tests/SDL3IntegrationTests.cpp
      ${WINDOW_TEST_SOURCES} ${IMGUI_SOURCES} src/System/AudioManager.cpp src/System/GameStateManager.cpp src/Util/ConfigManager.cpp)
    target_include_directories(sdl3-integration-tests PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(sdl3-integration-tests PRIVATE r2nes_host SDL3::SDL3-static zlibstatic)
    if(WIN32)
      target_link_libraries(sdl3-integration-tests PRIVATE d3d11 d3dcompiler ole32 windowscodecs uuid)
    endif()
    target_include_directories(sdl3-integration-tests PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    add_test(NAME sdl3-integration COMMAND sdl3-integration-tests)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdl3-test-data")
    set_tests_properties(sdl3-integration PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdl3-test-data"
      ENVIRONMENT "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy;SDL_RENDER_DRIVER=software"
      TIMEOUT 30)
  endif()

  function(add_core_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE r2nes_host)
    add_test(NAME ${test_name} COMMAND ${target} ${ARGN})
  endfunction()

  add_core_test(mapper002-tests tests/Mapper002Tests.cpp mapper002 "${CMAKE_CURRENT_BINARY_DIR}/mapper002-test-roms")
  add_core_test(rom-file-tests tests/RomFileTests.cpp rom-file "${CMAKE_CURRENT_BINARY_DIR}/rom-file-test-data")
  target_include_directories(rom-file-tests PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(rom-file-tests PRIVATE zlibstatic)
  add_core_test(apu-output-suppression-tests tests/APUOutputSuppressionTests.cpp apu-output-suppression)
  add_core_test(apu-frame-timing-tests tests/APUFrameTimingTests.cpp apu-frame-timing)
  add_core_test(apu-dmc-reload-tests tests/APUDmcReloadTests.cpp apu-dmc-reload)
  add_core_test(dmc-abort-tests tests/DmcAbortTests.cpp dmc-abort)
  add_core_test(state-security-tests tests/StateSecurityTests.cpp state-security "${CMAKE_CURRENT_BINARY_DIR}/state-security-test-data")
  add_core_test(hostile-state-tests tests/HostileStateTests.cpp hostile-state "${CMAKE_CURRENT_BINARY_DIR}/hostile-state-test-data")
  set_tests_properties(hostile-state PROPERTIES TIMEOUT 90)
  add_core_test(state-rollback-timing-tests tests/StateRollbackTimingTests.cpp state-rollback-timing "${CMAKE_CURRENT_BINARY_DIR}/state-rollback-timing-test-data")
  set_tests_properties(state-rollback-timing PROPERTIES TIMEOUT 30)
  add_core_test(mapper-state-security-tests tests/MapperStateSecurityTests.cpp mapper-state-security "${CMAKE_CURRENT_BINARY_DIR}/mapper-state-security-test-data")
  add_core_test(region-timing-tests tests/RegionTimingTests.cpp region-timing "${CMAKE_CURRENT_BINARY_DIR}/region-test-roms")
  add_core_test(mapper069-tests tests/Mapper069Tests.cpp mapper069 "${CMAKE_CURRENT_BINARY_DIR}/mapper069-test-roms")
  add_core_test(mapper023-tests tests/Mapper023Tests.cpp mapper023 "${CMAKE_CURRENT_BINARY_DIR}/mapper023-test-roms")
  add_core_test(mapper090-tests tests/Mapper090Tests.cpp mapper090)
  add_core_test(mapper071-tests tests/Mapper071Tests.cpp mapper071 "${CMAKE_CURRENT_BINARY_DIR}/mapper071-test-roms")
  add_core_test(mapper189-tests tests/Mapper189Tests.cpp mapper189 "${CMAKE_CURRENT_BINARY_DIR}/mapper189-test-roms")
  add_core_test(ppu-decay-tests tests/PPUDecayTests.cpp ppu-decay)
  add_core_test(ppu-sprite-overflow-tests tests/PPUSpriteOverflowTests.cpp ppu-sprite-overflow)
  add_core_test(cpu-cycle-tests tests/CPUCycleTests.cpp cpu-cycle "${CMAKE_CURRENT_BINARY_DIR}/cpu-cycle-test-roms")
  add_core_test(cpu-bus-tests tests/CpuBusTests.cpp cpu-bus)
  add_core_test(oam-dma-register-tests tests/OamDmaRegisterTests.cpp oam-dma-register)
  add_core_test(cpu-interrupt-tests tests/CPUInterruptTests.cpp cpu-interrupt "${CMAKE_CURRENT_BINARY_DIR}/cpu-interrupt-test-data")
  # Diagnóstico manual: a ROM externa não é distribuída nem exigida pelo CTest.
  add_executable(accuracycoin-runner tests/AccuracyCoinRunner.cpp)
  target_include_directories(accuracycoin-runner PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_link_libraries(accuracycoin-runner PRIVATE r2nes_host zlibstatic)
  add_core_test(cheat-tests tests/CheatTests.cpp cheats "${CMAKE_CURRENT_BINARY_DIR}/cheat-test-roms" "${CMAKE_CURRENT_SOURCE_DIR}/cheats/ED2465BE.cht")
  add_core_test(battery-tests tests/BatteryMapperTests.cpp battery "${CMAKE_CURRENT_BINARY_DIR}/battery-test-roms")
  add_core_test(mapper005-tests tests/Mapper005Tests.cpp mapper005 "${CMAKE_CURRENT_BINARY_DIR}/mapper005-test-roms")
  add_core_test(mapper007-tests tests/Mapper007Tests.cpp mapper007 "${CMAKE_CURRENT_BINARY_DIR}/mapper007-test-roms")
  add_core_test(mapper009-tests tests/Mapper009Tests.cpp mapper009 "${CMAKE_CURRENT_BINARY_DIR}/mapper009-test-roms")
  add_core_test(mapper011-tests tests/Mapper011Tests.cpp mapper011 "${CMAKE_CURRENT_BINARY_DIR}/mapper011-test-roms")
  add_core_test(sachen-tests tests/SachenTests.cpp sachen "${CMAKE_CURRENT_BINARY_DIR}/sachen-test-roms")
  add_core_test(mapper004-tests tests/Mapper004Tests.cpp mapper004 "${CMAKE_CURRENT_BINARY_DIR}/mapper004-test-roms")
  foreach(MMC3_ROM IN ITEMS 1-clocking 2-details 3-A12_clocking 4-scanline_timing 5-MMC3 6-MMC3_alt 6-MMC6)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nestest/blarg/${MMC3_ROM}.nes")
      add_test(NAME mapper004-rom-${MMC3_ROM} COMMAND mapper004-tests
        "${CMAKE_CURRENT_BINARY_DIR}/mapper004-${MMC3_ROM}"
        "${CMAKE_CURRENT_SOURCE_DIR}/nestest/blarg/${MMC3_ROM}.nes")
    endif()
  endforeach()
  add_core_test(mapper003-tests tests/Mapper003Tests.cpp mapper003 "${CMAKE_CURRENT_BINARY_DIR}/mapper003-test-roms")
  add_core_test(mapper001-tests tests/Mapper001Tests.cpp mapper001 "${CMAKE_CURRENT_BINARY_DIR}/mapper001-test-roms")
endif()
