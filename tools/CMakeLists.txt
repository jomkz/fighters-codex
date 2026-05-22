add_executable(dll_info dll_info.cpp)

target_link_libraries(dll_info PRIVATE fx_lib)

target_include_directories(dll_info PRIVATE
    ${CMAKE_BINARY_DIR}/include)

if(MSVC)
    target_compile_options(dll_info PRIVATE /W3 /WX-)
else()
    target_compile_options(dll_info PRIVATE -Wall -Wextra)
endif()
