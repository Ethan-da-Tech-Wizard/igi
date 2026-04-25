include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(igi_smoke_test
    main.cpp
    smoke_test.cpp
    daemon_test.cpp
    hotkey_listener_test.cpp
)

# Use GTest::gtest (not gtest_main) because we supply our own main()
# that initialises QCoreApplication before running tests, which is
# required for QSignalSpy and any other Qt primitives used in tests.
target_link_libraries(igi_smoke_test PRIVATE
    igi_core
    GTest::gtest
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(igi_smoke_test)
