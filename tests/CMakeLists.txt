include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(igi_smoke_test
    smoke_test.cpp
    daemon_test.cpp
)

target_link_libraries(igi_smoke_test PRIVATE
    igi_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(igi_smoke_test)
