add_executable(fx
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

target_link_libraries(fx PRIVATE fx_lib fx_warnings)

# stb_image needed in cli for PNG I/O
target_include_directories(fx SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/lib/vendor)
target_include_directories(fx PRIVATE ${CMAKE_BINARY_DIR}/include)
