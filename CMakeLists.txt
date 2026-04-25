cmake_minimum_required(VERSION 3.16)
project(IgiOCR VERSION 1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(IGI_BUILD_TESTS "Build the Igi unit test suite" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)

# Core library: shared between the igi executable and the test suite.
# Keeping logic in a library lets us link tests directly against the
# same translation units the binary ships with.
add_library(igi_core STATIC
    src/core/Daemon.cpp
)

# macOS-only: Objective-C++ sources for system framework integrations.
if(APPLE)
    target_sources(igi_core PRIVATE
        src/core/AppPolicy.mm
        src/core/HotkeyListener.mm
        src/core/Permissions.mm
    )

    target_link_libraries(igi_core PUBLIC
        "-framework AppKit"
        "-framework Carbon"
        "-framework ApplicationServices"
    )
endif()

target_include_directories(igi_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(igi_core SYSTEM PUBLIC
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPTONICA_INCLUDE_DIRS}
)

target_link_libraries(igi_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
)

target_link_directories(igi_core PUBLIC
    ${TESSERACT_LIBRARY_DIRS}
    ${LEPTONICA_LIBRARY_DIRS}
)

add_executable(igi
    src/main.cpp
)

target_link_libraries(igi PRIVATE igi_core)

if(IGI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
