# libFuzzer harnesses — built only by the fuzz preset (FX_BUILD_FUZZERS=ON,
# Clang). Each harness is one <name>.cpp implementing LLVMFuzzerTestOneInput.
#
# fx_add_fuzzer(<name>) wires up:
#   - the harness executable, hung off the `fuzzers` umbrella target
#   - a ctest smoke run (label "fuzz", `ctest --preset fuzz`) over the
#     committed seed corpus in corpus/<name>/ plus a working corpus in the
#     build tree; a <name>.dict next to this file is picked up automatically
#
# The Phase 4 rollout (#51) adds a parser harness by dropping in the .cpp
# and one fx_add_fuzzer line here. Findings (crash-*/oom-*/timeout-*) land
# in the build tree, never the source tree.

add_custom_target(fuzzers)

function(fx_add_fuzzer name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fx_lib fx_warnings)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    add_dependencies(fuzzers ${name})

    set(work_dir ${CMAKE_CURRENT_BINARY_DIR}/${name}-work)
    file(MAKE_DIRECTORY ${work_dir}/corpus)
    set(smoke_args
        -max_total_time=60
        -rss_limit_mb=2048
        -malloc_limit_mb=512
        -max_len=1048576
        -print_final_stats=1
        -artifact_prefix=${work_dir}/
        ${work_dir}/corpus)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name})
        list(APPEND smoke_args ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name})
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.dict)
        list(APPEND smoke_args -dict=${CMAKE_CURRENT_SOURCE_DIR}/${name}.dict)
    endif()

    add_test(NAME fuzz_smoke_${name} COMMAND ${name} ${smoke_args})
    set_tests_properties(fuzz_smoke_${name} PROPERTIES
        LABELS "fuzz"
        TIMEOUT 300)
endfunction()

fx_add_fuzzer(fuzz_ealib)
