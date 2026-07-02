# Driven by the cli_e2e_lib CTest (tests/CMakeLists.txt): round-trips a
# synthetic archive through the real fx binary — pack, ls, extract, unpack,
# patch — and byte-compares every output. Any failure fails the test.
# Expects FX (path to the fx executable) and WORK_DIR.

# Run fx and capture stdout; fail unless the exit code matches expect_rc.
function(run_fx expect_rc out_var)
    execute_process(
        COMMAND ${FX} ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    if(NOT rc EQUAL ${expect_rc})
        message(FATAL_ERROR "fx ${ARGN} exited ${rc} (expected ${expect_rc})\n${out}\n${err}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

function(compare_files a b)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E compare_files ${a} ${b}
        RESULT_VARIABLE rc)
    if(rc)
        message(FATAL_ERROR "byte mismatch: ${a} vs ${b}")
    endif()
endfunction()

# --- fixtures (12-char max names; pack truncates longer) -------------------
file(REMOVE_RECURSE ${WORK_DIR})
set(SRC ${WORK_DIR}/src)
file(MAKE_DIRECTORY ${SRC})
string(ASCII 10 13 255 1 mixed_bytes)
file(WRITE "${SRC}/ALPHA.BIN" "alpha payload ${mixed_bytes} end")
file(WRITE "${SRC}/beta.txt"  "beta: lowercase stored name\n")
file(WRITE "${SRC}/&LOOP.11K" "looping audio ${mixed_bytes}")

# --- pack: deterministic, name-sorted entry order ---------------------------
run_fx(0 out lib pack ${SRC} ${WORK_DIR}/test.LIB)

run_fx(0 out lib ls ${WORK_DIR}/test.LIB)
if(NOT out MATCHES "3 file\\(s\\)")
    message(FATAL_ERROR "ls did not report 3 file(s):\n${out}")
endif()
# Byte-sorted order: '&' (0x26) < 'A' (0x41) < 'b' (0x62)
if(NOT out MATCHES "&LOOP\\.11K.*ALPHA\\.BIN.*beta\\.txt")
    message(FATAL_ERROR "ls entries not in byte-sorted pack order:\n${out}")
endif()

# --- extract: case-insensitive lookup, forward-slash progress output --------
run_fx(0 out lib extract ${WORK_DIR}/test.LIB BETA.TXT -o ${WORK_DIR}/out_extract)
compare_files(${WORK_DIR}/out_extract/beta.txt ${SRC}/beta.txt)
if(NOT out MATCHES "out_extract/beta\\.txt")
    message(FATAL_ERROR "extract did not print a forward-slash destination:\n${out}")
endif()

# --- unpack: all entries, '&' sanitized to '_' -------------------------------
run_fx(0 out lib unpack ${WORK_DIR}/test.LIB ${WORK_DIR}/out_unpack)
compare_files(${WORK_DIR}/out_unpack/ALPHA.BIN ${SRC}/ALPHA.BIN)
compare_files(${WORK_DIR}/out_unpack/beta.txt ${SRC}/beta.txt)
compare_files("${WORK_DIR}/out_unpack/_LOOP.11K" "${SRC}/&LOOP.11K")

# --- patch: replace one entry, verify new bytes ------------------------------
run_fx(0 out lib patch ${WORK_DIR}/test.LIB ALPHA.BIN ${SRC}/beta.txt ${WORK_DIR}/patched.LIB)
run_fx(0 out lib extract ${WORK_DIR}/patched.LIB ALPHA.BIN -o ${WORK_DIR}/out_patched)
compare_files(${WORK_DIR}/out_patched/ALPHA.BIN ${SRC}/beta.txt)

# --- bare fx: nonzero exit, usage documents lib extract ----------------------
run_fx(1 out)
if(NOT out MATCHES "fx lib extract")
    message(FATAL_ERROR "usage text does not document lib extract:\n${out}")
endif()

message(STATUS "cli_e2e_lib: all checks passed")
