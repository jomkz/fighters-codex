include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(ft_tests
    test_ealib.cpp
)

target_link_libraries(ft_tests PRIVATE ft_lib Catch2::Catch2WithMain)

if(MSVC)
    target_compile_options(ft_tests PRIVATE /W3 /WX-)
endif()

include(CTest)
include(Catch)
catch_discover_tests(ft_tests)
