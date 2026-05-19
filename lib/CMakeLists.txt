add_library(ft_lib STATIC
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
)

target_include_directories(ft_lib
    PUBLIC  include
    PRIVATE vendor
)

if(MSVC)
    target_compile_options(ft_lib PRIVATE /W3 /WX-)
else()
    target_compile_options(ft_lib PRIVATE -Wall -Wextra)
endif()
