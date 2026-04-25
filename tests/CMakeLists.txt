find_package(GTest REQUIRED)

add_executable(igi_smoke_test
    main.cpp
    smoke_test.cpp
    daemon_test.cpp
    hotkey_listener_test.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Test)

# Use GTest::gtest (not gtest_main) because we supply our own main()
# that initialises QCoreApplication before running tests, which is
# required for QSignalSpy and any other Qt primitives used in tests.
target_link_libraries(igi_smoke_test PRIVATE
    igi_core
    Qt6::Test
    GTest::gtest
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(igi_smoke_test)
