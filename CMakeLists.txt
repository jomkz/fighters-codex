cmake_minimum_required(VERSION 3.21)
project(fighters-codex VERSION 0.8.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT on MSVC so the exe has zero runtime dependencies. Only when this
# is the top-level build; embedded builds inherit the host project's CRT.
if(MSVC AND PROJECT_IS_TOP_LEVEL)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Warning policy for every target in this repo; linked PRIVATE so it never
# propagates to consumers.
#
# FX_WERROR makes THIS repo's warnings errors, scoped to fx_warnings — so it
# never turns a third-party dependency's warning (a FetchContent SDL3, Catch2)
# into a build break. The macOS CI gate turns it on: AppleClang under -Werror is
# the project's portability canary (#155), catching what the Linux/Windows gates,
# which do not use -Werror, let through.
option(FX_WERROR "Treat this repo's own warnings as errors (not third-party deps)" OFF)
add_library(fx_warnings INTERFACE)
if(MSVC)
    target_compile_options(fx_warnings INTERFACE /W3 $<IF:$<BOOL:${FX_WERROR}>,/WX,/WX->)
else()
    target_compile_options(fx_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
    if(FX_WERROR)
        target_compile_options(fx_warnings INTERFACE -Werror)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Catch2's TEST_CASE expands __COUNTER__, which Clang's -Wpedantic
        # flags as a C2y extension at the expansion site in test files
        target_compile_options(fx_warnings INTERFACE -Wno-c2y-extensions)
    endif()
endif()

# The embeddable product: consumers add_subdirectory this repo and link fx::lib.
add_subdirectory(lib)

# fx_render: the shared renderer (software backend always; OpenGL backend added
# in fx_render #289). Embeddable like fx::lib — fxs and fxe both link it.
add_subdirectory(render)

# Everything below is built only when this repo is the top-level project.
if(PROJECT_IS_TOP_LEVEL)
    configure_file(
        "${CMAKE_SOURCE_DIR}/version.h.in"
        "${CMAKE_BINARY_DIR}/include/fx/version.h")

    enable_testing()

    add_subdirectory(cli)

    # fxs: SDL3 + OpenGL 3.3 + miniaudio on Linux, Windows and macOS (ADR-0001;
    # macOS uses a 4.1 core context, the platform's ≥3.3 superset — see #155).
    # OFF in the coverage/fuzz presets — no value in instrumented SDL3 builds.
    option(FX_BUILD_GUI "Build the fxs editor" ON)
    option(FX_SDL3_VENDORED "Force the pinned FetchContent SDL3 (static)" OFF)
    if(FX_BUILD_GUI)
        add_subdirectory(gui)
    endif()

    # fxe: the clean-room source port of the game executable (epic #280). Today this
    # builds only the generated declarations — compiling them is the first proof that
    # the reconstruction in db/ is coherent. See fxe/README.md.
    add_subdirectory(fxe)

    add_subdirectory(tools)
    add_subdirectory(tests)

    # libFuzzer harnesses; ON only in the fuzz preset (Clang-only)
    option(FX_BUILD_FUZZERS "Build libFuzzer harnesses (requires Clang)" OFF)
    if(FX_BUILD_FUZZERS)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "FX_BUILD_FUZZERS requires Clang — configure with the fuzz preset")
        endif()
        add_subdirectory(fuzz)
    endif()
endif()
