# Driven by the cli_e2e_install CTest (tests/CMakeLists.txt): synthesises a disc
# with the real fx binary (`fx esa pack`), then plans, installs and verifies it —
# `fx install` end to end, over media that contains no game bytes.
# Expects FX (path to the fx executable) and WORK_DIR.

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

# --- a synthetic Disc 1 -----------------------------------------------------
# `fx esa pack` puts every file under one label, so the script selects the whole
# label with the DOS `*.*` glob — the same directive the retail FA_README and
# FA_MISC lines use.
file(REMOVE_RECURSE ${WORK_DIR})
set(SRC  ${WORK_DIR}/src)
set(DISC ${WORK_DIR}/disc1)
set(DEST ${WORK_DIR}/install)
file(MAKE_DIRECTORY ${SRC} ${DISC})

string(ASCII 10 13 255 1 mixed_bytes)
file(WRITE "${SRC}/FA.EXE"     "not really an executable ${mixed_bytes}")
file(WRITE "${SRC}/CHAT.TXT"   "send to all\n")
file(WRITE "${SRC}/EXAMPLE.MT" "the shipped example mission text\n")

run_fx(0 out esa pack ${DISC}/SETUP.ESA FA_MISC
       ${SRC}/FA.EXE ${SRC}/CHAT.TXT ${SRC}/EXAMPLE.MT)

file(WRITE ${DISC}/SETUP.SSF
"COMPANY_NAME \"Jane's Combat Simulations\"
APP_NAME \"Fighters Anthology\"
DEFAULT_PATH \"\\\\JANES\\\\Fighters Anthology\"
INSTALL_SCRIPT \"MINSTALL.SSF\",\":0409:Minimal Install\"
INSTALL_SCRIPT \"FINSTALL.SSF\",\":0409:Full Install\"
")
file(WRITE ${DISC}/FINSTALL.SSF
"CREATE_FOLDERS \"[INSTALL_PATH]\"
INSTALL_FILES \"*.*\",\"FA_MISC\",\"[INSTALL_PATH]\"
SKIP_ON_REMOVE \"*.MT\"
REGEXE \"[INSTALL_PATH]\\\\FA.EXE\"
")
# The minimal script installs less — which is the only thing that distinguishes
# it, since the INSTALL_SCRIPT labels above are localised prose.
file(WRITE ${DISC}/MINSTALL.SSF
"CREATE_FOLDERS \"[INSTALL_PATH]\"
INSTALL_FILES \"CHAT.TXT\",\"FA_MISC\",\"[INSTALL_PATH]\"
SKIP_ON_REMOVE \"*.MT\"
")
# A loose LIB the archive does not carry: CD-resident by the rule, not a list.
file(WRITE ${DISC}/FA_4C.LIB "cd audio\n")

# --- plan: the dry run, and it refuses media it cannot fingerprint -----------
run_fx(1 out install plan ${DISC})
if(NOT out MATCHES "unknown")
    message(FATAL_ERROR "plan did not report the media as unknown:\n${out}")
endif()

run_fx(0 out install plan ${DISC} --any-media)
if(NOT out MATCHES "FINSTALL\\.SSF")
    message(FATAL_ERROR "plan did not choose the full script by size:\n${out}")
endif()
foreach(f FA.EXE CHAT.TXT EXAMPLE.MT FA_4C.LIB)
    if(NOT out MATCHES "copy +${f}")
        message(FATAL_ERROR "plan does not copy ${f}:\n${out}")
    endif()
endforeach()
if(NOT out MATCHES "REGEXE")
    message(FATAL_ERROR "plan does not report the unhonored REGEXE directive:\n${out}")
endif()
# plan is a dry run: it writes nothing.
if(EXISTS ${DEST})
    message(FATAL_ERROR "install plan created ${DEST}")
endif()

# --minimal picks the smaller script.
run_fx(0 out install plan ${DISC} --any-media --minimal)
if(out MATCHES "copy +FA\\.EXE")
    message(FATAL_ERROR "the minimal script installed FA.EXE:\n${out}")
endif()

# --- run + verify -----------------------------------------------------------
run_fx(0 out install run ${DISC} -d ${DEST} --any-media --verify)
if(NOT out MATCHES "verified")
    message(FATAL_ERROR "install run did not verify:\n${out}")
endif()
compare_files(${DEST}/FA.EXE     ${SRC}/FA.EXE)
compare_files(${DEST}/CHAT.TXT   ${SRC}/CHAT.TXT)
compare_files(${DEST}/EXAMPLE.MT ${SRC}/EXAMPLE.MT)
compare_files(${DEST}/FA_4C.LIB  ${DISC}/FA_4C.LIB)

run_fx(0 out install verify ${DISC} -d ${DEST} --any-media)

# --- the clobber guard ------------------------------------------------------
# The user edits their mission text and corrupts a game file; a re-install with
# --overwrite restores the game file and keeps the user's, because SKIP_ON_REMOVE
# "*.MT" marks .MT as something the game writes.
file(WRITE ${DEST}/EXAMPLE.MT "MY OWN MISSION TEXT\n")
file(WRITE ${DEST}/CHAT.TXT   "corrupted\n")

run_fx(0 out install run ${DISC} -d ${DEST} --any-media --overwrite)
compare_files(${DEST}/CHAT.TXT ${SRC}/CHAT.TXT)
file(READ ${DEST}/EXAMPLE.MT mt)
if(NOT mt MATCHES "MY OWN MISSION TEXT")
    message(FATAL_ERROR "--overwrite clobbered a SKIP_ON_REMOVE file")
endif()

# ...and verify now fails, because the file on disk is no longer the disc's.
run_fx(1 out install verify ${DISC} -d ${DEST} --any-media)

# --- json: the shape fxe's first-run reads ----------------------------------
run_fx(0 out install plan ${DISC} --any-media --json)
foreach(key "\"build\"" "\"items\"" "\"directives\"" "\"errors\"" "\"status\": \"copy\"")
    if(NOT out MATCHES ${key})
        message(FATAL_ERROR "--json output is missing ${key}:\n${out}")
    endif()
endforeach()

message(STATUS "cli_e2e_install: all checks passed")
