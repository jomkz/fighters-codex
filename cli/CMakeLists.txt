add_executable(ft
    main.cpp
    cmd_lib.cpp
    cmd_pic.cpp
    cmd_seq.cpp
    cmd_audio.cpp
    cmd_ot.cpp
    cmd_mission.cpp
    cmd_sh.cpp
    cmd_cb8.cpp
    cmd_raw.cpp
    cmd_sms.cpp
    cmd_t2.cpp
    cmd_plt.cpp
    cmd_pal.cpp
    cmd_inf.cpp
    cmd_hud.cpp
    cmd_lay.cpp
    cmd_fnt.cpp
    cmd_mus.cpp
    cmd_bi.cpp
    cmd_ai.cpp
)

target_link_libraries(ft PRIVATE ft_lib)

# stb_image needed in cli for PNG I/O
target_include_directories(ft PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/vendor
    ${CMAKE_BINARY_DIR}/include)

if(MSVC)
    target_compile_options(ft PRIVATE /W3 /WX-)
else()
    target_compile_options(ft PRIVATE -Wall -Wextra)
endif()
