include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(fx_tests
    test_ealib.cpp
    test_pic.cpp
    test_audio.cpp
    test_seq.cpp
    test_brf.cpp
    test_mission.cpp
    test_sh.cpp
    test_t2.cpp
    test_plt.cpp
)

target_link_libraries(fx_tests PRIVATE fx_lib Catch2::Catch2WithMain)

if(MSVC)
    target_compile_options(fx_tests PRIVATE /W3 /WX-)
endif()

include(CTest)
include(Catch)
catch_discover_tests(fx_tests)
