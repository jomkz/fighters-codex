add_executable(dll_info dll_info.cpp)

target_link_libraries(dll_info PRIVATE fx_lib fx_warnings)

target_include_directories(dll_info PRIVATE
    ${CMAKE_BINARY_DIR}/include)
