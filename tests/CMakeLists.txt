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

target_link_libraries(fx_tests PRIVATE fx_lib fx_warnings Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(fx_tests)

# Embedding smoke test: proves the documented consumer contract (api.md) --
# add_subdirectory of the repo root builds only fx_lib and links as fx::lib
# into a shared library, offline.
add_test(NAME embed_smoke
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/embed
        -DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}/embed-build
        -DREPO_DIR=${PROJECT_SOURCE_DIR}
        -DGENERATOR=${CMAKE_GENERATOR}
        -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/embed/run_embed_test.cmake)

# End-to-end round-trip through the real fx binary: pack, ls, extract,
# unpack, patch against a synthetic archive generated at test time.
add_test(NAME cli_e2e_lib
    COMMAND ${CMAKE_COMMAND}
        -DFX=$<TARGET_FILE:fx>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-e2e
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli/run_cli_test.cmake)
