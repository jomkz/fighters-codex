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
    cmd_fbc.cpp
    cmd_bin.cpp
    cmd_cam.cpp
    cmd_txt.cpp
    cmd_cfg.cpp
    cmd_dat.cpp
    cmd_mnu.cpp
    cmd_mt.cpp
    cmd_pts.cpp
    cmd_rgn.cpp
    cmd_ssf.cpp
    cmd_mc.cpp
    cmd_hgr.cpp
    cmd_dlg.cpp
)

target_link_libraries(fx PRIVATE fx_lib fx_warnings)

# UTF-8 active code page (#165): narrow argv and narrow CRT/Win32 path APIs
# speak UTF-8 on Windows 10 1903+ instead of the user's ANSI code page.
# CMake merges .manifest sources into the linker manifest via mt.exe.
if(MSVC)
    target_sources(fx PRIVATE ${PROJECT_SOURCE_DIR}/cmake/win-utf8.manifest)
endif()

# stb_image needed in cli for PNG I/O
target_include_directories(fx SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/lib/vendor)
target_include_directories(fx PRIVATE ${CMAKE_BINARY_DIR}/include)
