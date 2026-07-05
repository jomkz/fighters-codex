add_library(fx_lib STATIC
    src/blast.cpp
    src/pal.cpp
    src/ealib.cpp
    src/pic.cpp
    src/seq.cpp
    src/audio.cpp
    src/brf.cpp
    src/ot.cpp
    src/mission.cpp
    src/sh.cpp
    src/cb8.cpp
    src/raw.cpp
    src/sms.cpp
    src/t2.cpp
    src/plt.cpp
    src/inf.cpp
    src/hud.cpp
    src/pe.cpp
    src/lay.cpp
    src/fnt.cpp
    src/bi.cpp
    src/ai.cpp
    src/fbc.cpp
    src/bin.cpp
    src/cam.cpp
    src/txt.cpp
    src/cfg.cpp
    src/dat.cpp
    src/mnu.cpp
)

add_library(fx::lib ALIAS fx_lib)

target_include_directories(fx_lib PUBLIC include)
target_include_directories(fx_lib SYSTEM PRIVATE vendor)

target_link_libraries(fx_lib PRIVATE fx_warnings)

# fa-bridge links fx_lib into a shared plugin, which needs PIC on Linux
set_target_properties(fx_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
