# fxe — the clean-room source port of the game executable (epic #280, ADR-0002).
#
# This is the port's first stage: the DECLARATIONS, generated from db/ by
# tools/gen_fxe.py and committed (the generator is the source of truth; see
# fxe/README.md). There is no runtime yet — no entry point, no fx_render hook
# (#292), no camera (#387).
#
# The target below exists to COMPILE the generated headers, and that is the whole
# point of it. fx_lib proves a format is understood by round-tripping its bytes;
# the equivalent proof at the executable level starts here — a declaration that is
# not valid C++ is not a reconstruction of anything. It is cheap (declarations
# only, no linking) and it runs on every build, so a db/ change that emits a
# malformed type stops the build instead of rotting.
#
# It has already earned its keep: the first generated tree kept the MSVC calling
# convention inside function-pointer parameters, which gcc and clang reject.

add_library(fxe_generated STATIC compile_check.cpp)

target_include_directories(fxe_generated PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The generated headers are held to the same warning bar as the rest of the tree:
# they are the port's public surface, not scratch output.
target_link_libraries(fxe_generated PRIVATE fx_warnings)

set_target_properties(fxe_generated PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
