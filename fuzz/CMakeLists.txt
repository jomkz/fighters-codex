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
fx_add_fuzzer(fuzz_blast)
fx_add_fuzzer(fuzz_pe)

# Batch 2 (#117): images and A/V
fx_add_fuzzer(fuzz_pic)
fx_add_fuzzer(fuzz_cb8)
fx_add_fuzzer(fuzz_raw)
fx_add_fuzzer(fuzz_fnt)
fx_add_fuzzer(fuzz_seq)
fx_add_fuzzer(fuzz_audio)

# Batch 3 (#118): game data — round-trip codecs and read-only parsers
fx_add_fuzzer(fuzz_brf)
fx_add_fuzzer(fuzz_mission)
fx_add_fuzzer(fuzz_sh)
fx_add_fuzzer(fuzz_t2)
fx_add_fuzzer(fuzz_plt)
fx_add_fuzzer(fuzz_hud)
fx_add_fuzzer(fuzz_inf)
fx_add_fuzzer(fuzz_lay)
fx_add_fuzzer(fuzz_sms)
fx_add_fuzzer(fuzz_bi)

# Batch 4 (#318): epic #49 formats + the new write paths
fx_add_fuzzer(fuzz_txt)
fx_add_fuzzer(fuzz_cfg)
fx_add_fuzzer(fuzz_dat)
fx_add_fuzzer(fuzz_rgn)
fx_add_fuzzer(fuzz_ssf)
fx_add_fuzzer(fuzz_fbc)
fx_add_fuzzer(fuzz_vdo)
fx_add_fuzzer(fuzz_cam)
fx_add_fuzzer(fuzz_mnu)
fx_add_fuzzer(fuzz_dlg)
fx_add_fuzzer(fuzz_pts)
fx_add_fuzzer(fuzz_mc)
fx_add_fuzzer(fuzz_hgr)
fx_add_fuzzer(fuzz_xmi)
fx_add_fuzzer(fuzz_mt)
fx_add_fuzzer(fuzz_bin)

# Matrix-completeness sweep (#186 gate): the last uncovered codec rows
fx_add_fuzzer(fuzz_ai)
fx_add_fuzzer(fuzz_mus)
fx_add_fuzzer(fuzz_pal)
fx_add_fuzzer(fuzz_effect)
