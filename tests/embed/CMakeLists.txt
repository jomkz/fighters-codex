cmake_minimum_required(VERSION 3.21)
project(fx-embed-smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The documented consumer contract (docs/api.md): add the repo root, link fx::lib.
add_subdirectory(${FX_REPO_DIR} fx_lib_build)

# Shared library mirrors fa-content's plugin and requires fx_lib to be PIC.
add_library(consumer SHARED consumer.cpp)
target_link_libraries(consumer PRIVATE fx::lib)
