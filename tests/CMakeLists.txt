include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(fx_tests
    test_ealib.cpp
    test_pic.cpp
    test_pal.cpp
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

# The release workflow builds its GitHub Release body with
# scripts/extract-changelog.py; exercising it here keeps it from rotting
# between tags. 0.3.0 is a frozen section; 0.1.0 is the oldest (its section
# ends at the link definitions, the parsing edge case). Python3 lookup is
# QUIET — machines without Python must configure cleanly.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME changelog_extract
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/scripts/extract-changelog.py" "0.3.0")
    set_tests_properties(changelog_extract PROPERTIES
        PASS_REGULAR_EXPRESSION "fighters-codex")
    add_test(NAME changelog_extract_oldest
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/scripts/extract-changelog.py" "0.1.0")
    set_tests_properties(changelog_extract_oldest PROPERTIES
        PASS_REGULAR_EXPRESSION "Initial release")

    # Docs currency (docs/spec-authoring.md): spec front-matter, template
    # conformance, and the generated STATUS.md matrix are checked on every
    # preset leg, not just the dedicated CI docs job — a codec change that
    # invalidates a spec's claims fails ctest locally too.
    add_test(NAME check_status_selftest
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tools/check_status.py" --self-test)
    add_test(NAME check_status_docs
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tools/check_status.py" --check)
    set_tests_properties(check_status_selftest check_status_docs
        PROPERTIES LABELS "docs")
endif()

# Real-asset integration mode: registered only when FX_FA_ROOT points at a
# licensed FA install (cache variable or environment). Verifies every
# extracted byte against the committed manifest; a manifest generated on one
# OS and verified on the other proves cross-platform byte-identity (epic #42).
# Python3 lookup stays inside this block — machines without the assets (or
# Python) must configure cleanly.
set(FX_FA_ROOT "" CACHE PATH "Licensed FA install for real-asset integration tests")
if(NOT FX_FA_ROOT AND DEFINED ENV{FX_FA_ROOT})
    set(FX_FA_ROOT "$ENV{FX_FA_ROOT}")
endif()
if(FX_FA_ROOT)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME fa_extract_manifest
        COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/integration/fa_manifest.py" verify
            --fx "$<TARGET_FILE:fx>"
            --fa-root "${FX_FA_ROOT}"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/integration/fa-extract.sha256"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/fa-extract-work")
    set_tests_properties(fa_extract_manifest PROPERTIES
        LABELS "integration"
        TIMEOUT 3600)
endif()
