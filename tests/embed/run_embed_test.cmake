# Driven by the embed_smoke CTest (tests/CMakeLists.txt): configures and builds
# the consumer project in tests/embed/ against the repo root. Any failure fails
# the test. Expects SRC_DIR, BIN_DIR, REPO_DIR, GENERATOR, PLATFORM,
# CXX_COMPILER, CONFIG.

set(configure_args
    -S ${SRC_DIR} -B ${BIN_DIR}
    -G ${GENERATOR}
    -DFX_REPO_DIR=${REPO_DIR})
if(PLATFORM)
    list(APPEND configure_args -A ${PLATFORM})
endif()
if(NOT GENERATOR MATCHES "Visual Studio")
    list(APPEND configure_args
        -DCMAKE_CXX_COMPILER=${CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CONFIG})
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} ${configure_args}
    RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "embed consumer configure failed (${rc})")
endif()

set(build_args --build ${BIN_DIR})
if(CONFIG)
    list(APPEND build_args --config ${CONFIG})
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} ${build_args}
    RESULT_VARIABLE rc)
if(rc)
    message(FATAL_ERROR "embed consumer build failed (${rc})")
endif()
