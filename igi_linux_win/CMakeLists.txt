cmake_minimum_required(VERSION 3.16)
project(IgiOCR VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(IGI_BUILD_TESTS "Build the Igi unit test suite" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TESSERACT REQUIRED tesseract)
    pkg_check_modules(LEPTONICA REQUIRED lept)

    # Link X11 for active window detection and global hotkeys
    find_package(X11 REQUIRED)
endif()

if(WIN32)
    # Try pkg-config if available (e.g. inside MSYS2)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(TESSERACT tesseract)
        pkg_check_modules(LEPTONICA lept)
    endif()

    # Manual search fallback
    if(NOT TESSERACT_FOUND)
        find_path(TESSERACT_INCLUDE_DIRS NAMES tesseract/baseapi.h)
        find_library(TESSERACT_LIBRARIES NAMES tesseract)
    endif()
    if(NOT LEPTONICA_FOUND)
        find_path(LEPTONICA_INCLUDE_DIRS NAMES leptonica/allheaders.h)
        find_library(LEPTONICA_LIBRARIES NAMES lept leptonica)
    endif()
endif()

# Core static library - compiles cross-platform engine logic
add_library(igi_core STATIC
    src/core/Daemon.cpp
    src/core/ScreenGeometry.cpp
    src/core/Permissions_stub.cpp
    src/core/ScreenCapture_qt.cpp
    src/ocr/ImageConverter.cpp
    src/ocr/OcrEngine.cpp
    src/search/FuzzySearch.cpp
    src/SearchSession.cpp
    src/ui/SearchOverlay.cpp
    src/ui/CaptureFlashOverlay.cpp
)

if(UNIX AND NOT APPLE)
    target_sources(igi_core PRIVATE
        src/core/ActiveWindowInspector_linux.cpp
        src/core/HotkeyListener_linux.cpp
        src/core/SecurityGuard_linux.cpp
    )
    target_link_libraries(igi_core PUBLIC
        X11::X11
    )
elseif(WIN32)
    target_sources(igi_core PRIVATE
        src/core/ActiveWindowInspector_win.cpp
        src/core/HotkeyListener_win.cpp
        src/core/SecurityGuard_win.cpp
    )
    target_link_libraries(igi_core PUBLIC
        dwmapi
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
    Qt6::Concurrent
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
)

if(TESSERACT_LIBRARY_DIRS)
    target_link_directories(igi_core PUBLIC ${TESSERACT_LIBRARY_DIRS})
endif()
if(LEPTONICA_LIBRARY_DIRS)
    target_link_directories(igi_core PUBLIC ${LEPTONICA_LIBRARY_DIRS})
endif()

# Main executable
add_executable(igi
    src/main.cpp
)

target_link_libraries(igi PRIVATE igi_core)

# Tests suite
if(IGI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
