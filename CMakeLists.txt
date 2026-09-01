cmake_minimum_required(VERSION 3.24)

project(PhalanxRecomp VERSION 0.1.0 LANGUAGES C CXX OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
# Dear ImGui (the in-game menu) is the only C++ in this project.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(_PHALANX_SNESRECOMP_SUBMODULE "${CMAKE_SOURCE_DIR}/snesrecomp")
set(SNESRECOMP_ROOT "${_PHALANX_SNESRECOMP_SUBMODULE}" CACHE PATH
    "Path to the snesrecomp dependency checkout")
option(PHALANX_ALLOW_UNPINNED_SNESRECOMP
       "Allow an external, dirty, or mismatched snesrecomp checkout" OFF)

if(NOT EXISTS "${SNESRECOMP_ROOT}/runner/runner.cmake")
    message(FATAL_ERROR
        "snesrecomp is missing. Run: git submodule update --init snesrecomp")
endif()

if(NOT PHALANX_ALLOW_UNPINNED_SNESRECOMP)
    get_filename_component(_PHALANX_SNESRECOMP_ROOT_REAL
                           "${SNESRECOMP_ROOT}" REALPATH)
    get_filename_component(_PHALANX_SNESRECOMP_SUBMODULE_REAL
                           "${_PHALANX_SNESRECOMP_SUBMODULE}" REALPATH)
    if(NOT _PHALANX_SNESRECOMP_ROOT_REAL STREQUAL
           _PHALANX_SNESRECOMP_SUBMODULE_REAL)
        message(FATAL_ERROR
            "SNESRECOMP_ROOT points outside the pinned submodule. "
            "Use -DPHALANX_ALLOW_UNPINNED_SNESRECOMP=ON only when "
            "intentionally developing framework changes.")
    endif()

    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        message(FATAL_ERROR
            "Git is required to verify the pinned snesrecomp submodule.")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}"
                ls-files --stage -- snesrecomp
        OUTPUT_VARIABLE _PHALANX_SNESRECOMP_GITLINK
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _PHALANX_SNESRECOMP_GITLINK_RC
        ERROR_QUIET)
    if(NOT _PHALANX_SNESRECOMP_GITLINK_RC EQUAL 0 OR
       NOT _PHALANX_SNESRECOMP_GITLINK MATCHES
           "^160000 +([0-9a-fA-F]+) +0")
        message(FATAL_ERROR
            "Could not read the pinned snesrecomp gitlink from this checkout.")
    endif()
    set(_PHALANX_SNESRECOMP_PIN "${CMAKE_MATCH_1}")

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SNESRECOMP_ROOT}" rev-parse HEAD
        OUTPUT_VARIABLE _PHALANX_SNESRECOMP_HEAD
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _PHALANX_SNESRECOMP_HEAD_RC)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SNESRECOMP_ROOT}"
                status --porcelain=v1 --untracked-files=no
                --ignore-submodules=none
        OUTPUT_VARIABLE _PHALANX_SNESRECOMP_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _PHALANX_SNESRECOMP_STATUS_RC)
    if(NOT _PHALANX_SNESRECOMP_HEAD_RC EQUAL 0 OR
       NOT _PHALANX_SNESRECOMP_STATUS_RC EQUAL 0 OR
       NOT _PHALANX_SNESRECOMP_HEAD STREQUAL _PHALANX_SNESRECOMP_PIN OR
       NOT _PHALANX_SNESRECOMP_STATUS STREQUAL "")
        message(FATAL_ERROR
            "snesrecomp is not the clean pinned revision "
            "${_PHALANX_SNESRECOMP_PIN}. Run: "
            "git submodule update --init snesrecomp")
    endif()
endif()

include("${SNESRECOMP_ROOT}/runner/runner.cmake")
find_package(SDL2 REQUIRED)

set(PHALANX_IMGUI_ROOT "${CMAKE_SOURCE_DIR}/third_party/imgui")
if(NOT EXISTS "${PHALANX_IMGUI_ROOT}/imgui.cpp")
    message(FATAL_ERROR
        "Dear ImGui is missing. Run: git submodule update --init third_party/imgui")
endif()

# The menu's toolkit. src/imgui_sdl2_backend.cpp wraps the SDL2 platform
# backend so the app links no SDL controller API; see the file for why.
add_library(phalanx_imgui STATIC
    "${PHALANX_IMGUI_ROOT}/imgui.cpp"
    "${PHALANX_IMGUI_ROOT}/imgui_draw.cpp"
    "${PHALANX_IMGUI_ROOT}/imgui_tables.cpp"
    "${PHALANX_IMGUI_ROOT}/imgui_widgets.cpp"
    "${PHALANX_IMGUI_ROOT}/backends/imgui_impl_sdlrenderer2.cpp"
    src/imgui_sdl2_backend.cpp
)
target_include_directories(phalanx_imgui PUBLIC
    "${PHALANX_IMGUI_ROOT}"
    "${PHALANX_IMGUI_ROOT}/backends"
    ${SDL2_INCLUDE_DIRS}
)
target_compile_definitions(phalanx_imgui PUBLIC IMGUI_DISABLE_DEMO_WINDOWS=1)
if(NOT MSVC)
    target_compile_options(phalanx_imgui PRIVATE -w)
endif()
if(TARGET SDL2::SDL2)
    target_link_libraries(phalanx_imgui PUBLIC SDL2::SDL2)
else()
    target_link_libraries(phalanx_imgui PUBLIC ${SDL2_LIBRARIES})
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/src/build_config.h.in"
    "${CMAKE_BINARY_DIR}/generated/build_config.h"
    @ONLY)

file(GLOB PHALANX_GENERATED CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/gen/*.c")
if(NOT PHALANX_GENERATED)
    message(FATAL_ERROR
        "Generated code is missing. Run: bash tools/regen.sh")
endif()

add_executable(Phalanx MACOSX_BUNDLE
    ${SNESRECOMP_RUNNER_SOURCES}
    ${PHALANX_GENERATED}
    src/main.c
    src/input.c
    src/machine.c
    src/savestate.c
    src/save_slots.c
    src/ui.cpp
    src/platform_macos.m
)

target_include_directories(Phalanx PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/recomp"
    "${CMAKE_BINARY_DIR}/generated"
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_compile_definitions(Phalanx PRIVATE
    SDL_MAIN_HANDLED=1
    SMW_DISABLE_LM=1
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
    SNESRECOMP_LLE_BOUNCE_DEFAULT=0
    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
)

if(NOT MSVC)
    target_compile_options(Phalanx PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -Wno-implicit-function-declaration)
    set_source_files_properties(${PHALANX_GENERATED}
        PROPERTIES COMPILE_OPTIONS "-w")
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(Phalanx PRIVATE SDL2::SDL2)
else()
    target_link_libraries(Phalanx PRIVATE ${SDL2_LIBRARIES})
endif()

target_link_libraries(Phalanx PRIVATE
    m
    phalanx_imgui
    ${SNESRECOMP_RUNNER_LIBRARIES}
    "-framework AppKit"
    "-framework Foundation"
    "-framework GameController"
    "-framework UniformTypeIdentifiers"
)

set_source_files_properties(src/platform_macos.m
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

set_target_properties(Phalanx PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.navjack.phalanx-recompiled"
    MACOSX_BUNDLE_BUNDLE_NAME "Phalanx"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist.in"
    OUTPUT_NAME "Phalanx"
)

set(PHALANX_ORACLE_SOURCES
    ${SNESRECOMP_ROOT}/runner/src/audio_trace.c
    ${SNESRECOMP_ROOT}/runner/src/crc32.c
    ${SNESRECOMP_ROOT}/runner/src/sha256.c
    ${SNESRECOMP_ROOT}/runner/src/util.c
    ${SNESRECOMP_ROOT}/runner/src/snes/apu.c
    ${SNESRECOMP_ROOT}/runner/src/snes/cart.c
    ${SNESRECOMP_ROOT}/runner/src/snes/cpu.c
    ${SNESRECOMP_ROOT}/runner/src/snes/dma.c
    ${SNESRECOMP_ROOT}/runner/src/snes/dsp.c
    ${SNESRECOMP_ROOT}/runner/src/snes/ppu.c
    ${SNESRECOMP_ROOT}/runner/src/snes/ppu_old.c
    ${SNESRECOMP_ROOT}/runner/src/snes/ws_shadow.c
    ${SNESRECOMP_ROOT}/runner/src/snes/snes.c
    ${SNESRECOMP_ROOT}/runner/src/snes/snes_other.c
    ${SNESRECOMP_ROOT}/runner/src/snes/spc.c
    ${SNESRECOMP_ROOT}/runner/src/snes/superfx.c
    ${SNESRECOMP_ROOT}/runner/src/snes/interp816.c
    ${SNESRECOMP_ROOT}/runner/src/snes/color_lut.c
)

add_executable(phalanx_oracle EXCLUDE_FROM_ALL
    tests/oracle.c
    src/input.c
    ${PHALANX_ORACLE_SOURCES}
)
target_include_directories(phalanx_oracle PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
)
target_compile_definitions(phalanx_oracle PRIVATE
    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
)
if(NOT MSVC)
    target_compile_options(phalanx_oracle PRIVATE
        -w -Wno-implicit-function-declaration)
endif()
target_link_libraries(phalanx_oracle PRIVATE m)

# Save-state round trip. Runs the real machine, so it needs the ROM as its
# only argument; tools/build-macos.sh supplies it.
add_executable(phalanx_savestate_test EXCLUDE_FROM_ALL
    tests/savestate_test.c
    ${SNESRECOMP_RUNNER_SOURCES}
    ${PHALANX_GENERATED}
    src/input.c
    src/machine.c
    src/savestate.c
    src/save_slots.c
)
target_include_directories(phalanx_savestate_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/recomp"
    "${CMAKE_BINARY_DIR}/generated"
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)
target_compile_definitions(phalanx_savestate_test PRIVATE
    SDL_MAIN_HANDLED=1
    SMW_DISABLE_LM=1
    SYSTEM_VOLUME_MIXER_AVAILABLE=0
    SNESRECOMP_LLE_BOUNCE_DEFAULT=0
    SNESRECOMP_TRACE=0
    SNESRECOMP_REVERSE_DEBUG=0
)
if(NOT MSVC)
    target_compile_options(phalanx_savestate_test PRIVATE
        -w -Wno-implicit-function-declaration)
endif()
if(TARGET SDL2::SDL2)
    target_link_libraries(phalanx_savestate_test PRIVATE SDL2::SDL2)
else()
    target_link_libraries(phalanx_savestate_test PRIVATE ${SDL2_LIBRARIES})
endif()
target_link_libraries(phalanx_savestate_test PRIVATE
    m ${SNESRECOMP_RUNNER_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
    add_executable(phalanx_input_test
        tests/input_test.c
        src/input.c
    )
    target_include_directories(phalanx_input_test PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
    )
    add_test(NAME phalanx_input_mapping COMMAND phalanx_input_test)

    # No ROM and no display: the menu runs against SDL's dummy video driver.
    add_executable(phalanx_menu_test
        tests/menu_test.c
        src/ui.cpp
        src/save_slots.c
    )
    target_include_directories(phalanx_menu_test PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        ${SDL2_INCLUDE_DIRS}
    )
    target_compile_definitions(phalanx_menu_test PRIVATE SDL_MAIN_HANDLED=1)
    target_link_libraries(phalanx_menu_test PRIVATE phalanx_imgui)
    if(TARGET SDL2::SDL2)
        target_link_libraries(phalanx_menu_test PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(phalanx_menu_test PRIVATE ${SDL2_LIBRARIES})
    endif()
    add_test(NAME phalanx_menu COMMAND phalanx_menu_test)
endif()
