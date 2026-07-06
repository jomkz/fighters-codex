# fx_render — shared backend-agnostic renderer (OpenGL + software backends).
# Core + software backend have no dependencies and build everywhere; the
# OpenGL backend is added under a GL-available guard in fx_render #289.
add_library(fx_render STATIC
    src/fa.cpp
    src/render.cpp
    src/software.cpp
)

add_library(fx::render ALIAS fx_render)

target_include_directories(fx_render PUBLIC include)

target_link_libraries(fx_render PRIVATE fx_warnings)

# Consumers (fxs, fxe, and potentially fa-bridge) link this into shared
# libraries / plugins, which need PIC on Linux.
set_target_properties(fx_render PROPERTIES POSITION_INDEPENDENT_CODE ON)
