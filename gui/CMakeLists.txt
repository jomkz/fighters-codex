set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(ft-gui WIN32
    src/main.cpp
    src/app.cpp
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
    ${IMGUI_SOURCES}
)

target_include_directories(ft-gui PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_SOURCE_DIR}/lib/vendor
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(ft-gui PRIVATE
    ft_lib
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dwmapi.lib
)

if(MSVC)
    target_compile_options(ft-gui PRIVATE /W3 /WX-)
    # Enable large address awareness and set subsystem to Windows
    set_target_properties(ft-gui PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
