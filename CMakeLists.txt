cmake_minimum_required(VERSION 3.16)
project(IgiOCR VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Find Tesseract & Leptonica
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)

# Set up the executable
add_executable(igi
    src/main.cpp
)

# Include directories for Tesseract/Leptonica
target_include_directories(igi PRIVATE
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPTONICA_INCLUDE_DIRS}
)

# Link Qt, Tesseract, and Leptonica
target_link_libraries(igi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
)
