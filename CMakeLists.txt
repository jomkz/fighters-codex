cmake_minimum_required(VERSION 3.20)
project(fighters-toolkit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT on MSVC so the exe has zero runtime dependencies
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/version.h.in"
    "${CMAKE_BINARY_DIR}/include/ft/version.h")

enable_testing()

add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(gui)
add_subdirectory(tools)
add_subdirectory(tests)
