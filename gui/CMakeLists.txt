# SDL3 acquisition: system package first (Fedora: SDL3-devel), else a pinned,
# checksummed release tarball built as a static library — see ADR-0001.
# FX_SDL3_VENDORED=ON (CI/release) forces the pinned path so shipped artifacts
# never silently pick up a system SDL3.
if(NOT FX_SDL3_VENDORED)
    find_package(SDL3 QUIET CONFIG)
endif()
if(NOT TARGET SDL3::SDL3)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz
        URL_HASH SHA256=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7
    )
    FetchContent_MakeAvailable(SDL3)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

# Vendored code compiled without the repo warning policy.
add_library(fx_gui_vendor STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    src/platform/gl.cpp
)
target_include_directories(fx_gui_vendor PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)
target_link_libraries(fx_gui_vendor PUBLIC SDL3::SDL3)

# fx_render's OpenGL backend (render/src/gl_backend.cpp) is compiled here, where
# the glad loader lives; its glad function pointers are resolved from
# fx_gui_vendor's gl.cpp at the final fx-gui link (fx_render_gl is listed before
# fx_gui_vendor below). Core fx_render stays dependency-free. See fx_render #289.
add_library(fx_render_gl STATIC
    ${CMAKE_SOURCE_DIR}/render/src/gl_backend.cpp
)
target_include_directories(fx_render_gl PUBLIC ${CMAKE_SOURCE_DIR}/render/include)
target_include_directories(fx_render_gl SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
target_link_libraries(fx_render_gl PUBLIC fx_render PRIVATE fx_warnings)
set_target_properties(fx_render_gl PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(fx-gui WIN32
    src/main.cpp
    src/app.cpp
    src/palettes.cpp
    src/platform/audio_player.cpp
    src/platform/dialog_queue.cpp
    src/platform/dialogs.cpp
    src/platform/fonts.cpp
    src/platform/texture.cpp
    src/platform/theme.cpp
    src/platform/window.cpp
    src/panels/lib_browser.cpp
    src/panels/editor_host.cpp
    src/panels/preview.cpp
    src/editors/brf_editor.cpp
    src/editors/pic_editor.cpp
    src/editors/audio_editor.cpp
    src/editors/mission_editor.cpp
    src/editors/seq_editor.cpp
    src/editors/inf_editor.cpp
    src/editors/plt_editor.cpp
    src/editors/raw_viewer.cpp
    src/editors/sh_editor.cpp
    src/editors/txt_editor.cpp
    src/editors/bin_editor.cpp
    src/editors/lay_editor.cpp
    src/editors/hud_editor.cpp
    src/editors/mus_editor.cpp
    src/editors/fnt_editor.cpp
    src/editors/cb8_editor.cpp
    src/editors/ai_editor.cpp
    src/editors/xmi_editor.cpp
    src/editors/vdo_editor.cpp
    src/editors/cam_editor.cpp
    src/editors/pal_editor.cpp
)

target_include_directories(fx-gui PRIVATE
    src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_BINARY_DIR}/include
)
target_include_directories(fx-gui SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/vendor
)

target_link_libraries(fx-gui PRIVATE
    fx_lib
    fx_render_gl
    fx_gui_vendor
    fx_warnings
)

# Headless render smoke: three frames under Xvfb/llvmpipe in CI (offscreen
# driver as fallback). Linux-only — GitHub's Windows runners expose no
# GL 3.3; Windows gui coverage is the build plus the display-free gui_tests.
if(UNIX)
    add_test(NAME gui_smoke COMMAND fx-gui --smoke)
    set_tests_properties(gui_smoke PROPERTIES
        LABELS "gui"
        TIMEOUT 60
        # X11/dbus allocations at exit are noise on the asan-ubsan leg
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
