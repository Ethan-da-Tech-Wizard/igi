cmake_minimum_required(VERSION 3.16)
project(IgiOCR VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Objective-C++ is macOS-only; enabling it on Linux fails the
# CMake compiler probe.
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(IGI_BUILD_TESTS "Build the Igi unit test suite" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPTONICA REQUIRED lept)

# Core library: shared between the igi executable and the test suite.
# Keeping logic in a library lets us link tests directly against the
# same translation units the binary ships with.
add_library(igi_core STATIC
    src/core/Daemon.cpp
    src/core/ScreenGeometry.cpp
    src/ocr/ImageConverter.cpp
    src/ocr/OcrEngine.cpp
    src/search/FuzzySearch.cpp
    src/SearchSession.cpp
    src/ui/SearchOverlay.cpp
    src/ui/CaptureFlashOverlay.cpp
)

# macOS-only: Objective-C++ sources for system framework integrations.
if(APPLE)
    target_sources(igi_core PRIVATE
        src/core/AppPolicy.mm
        src/core/ActiveWindowInspector.mm
        src/core/HotkeyListener.mm
        src/core/Permissions.mm
        src/core/ScreenCapture.mm
        src/core/SecurityGuard.mm
    )

    target_link_libraries(igi_core PUBLIC
        "-framework AppKit"
        "-framework Carbon"
        "-framework ApplicationServices"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework Security"
        "-framework IOKit"
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

target_link_directories(igi_core PUBLIC
    ${TESSERACT_LIBRARY_DIRS}
    ${LEPTONICA_LIBRARY_DIRS}
)

add_executable(igi MACOSX_BUNDLE
    src/main.cpp
)

target_link_libraries(igi PRIVATE igi_core)

# ── macOS app bundle packaging (Chunk 7) ────────────────────────────────────
if(APPLE)
    # Embed Info.plist and Icon.
    set_target_properties(igi PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist"
        MACOSX_BUNDLE_ICON_FILE  "AppIcon.icns"
        MACOSX_BUNDLE_BUNDLE_NAME          "Igi"
        MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.igi.ocrsearch"
    )

    # Copy the app icon into the bundle's Resources directory.
    set_source_files_properties("resources/AppIcon.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(igi PRIVATE "resources/AppIcon.icns")

    # ── Locate tessdata ──────────────────────────────────────────────────────
    # Prefer an explicit override (useful in CI with a custom TESSDATA_PREFIX),
    # then fall back to the pkg-config prefix, then the Homebrew default.
    if(DEFINED TESSDATA_PREFIX AND EXISTS "${TESSDATA_PREFIX}/tessdata")
        set(IGI_TESSDATA_DIR "${TESSDATA_PREFIX}/tessdata")
    elseif(EXISTS "${TESSERACT_PREFIX}/share/tessdata")
        set(IGI_TESSDATA_DIR "${TESSERACT_PREFIX}/share/tessdata")
    else()
        # Ask pkg-config for the prefix by stripping /lib from LIBRARY_DIRS
        list(GET TESSERACT_LIBRARY_DIRS 0 _tess_libdir)
        get_filename_component(_tess_prefix "${_tess_libdir}" DIRECTORY)
        set(IGI_TESSDATA_DIR "${_tess_prefix}/share/tessdata")
    endif()

    if(NOT EXISTS "${IGI_TESSDATA_DIR}")
        message(WARNING
            "[igi] tessdata not found at '${IGI_TESSDATA_DIR}'. "
            "The bundle will build, but Tesseract will fail at runtime unless "
            "TESSDATA_PREFIX is set or tessdata is installed via Homebrew. "
            "Pass -DTESSDATA_PREFIX=/path/to/tessdata_parent to cmake to fix.")
    else()
        message(STATUS "[igi] Bundling tessdata from: ${IGI_TESSDATA_DIR}")
    endif()

    # Copy eng.traineddata (and osd) into the bundle's Resources/tessdata/.
    # We only bundle English; additional languages can be added here.
    foreach(_tdata eng.traineddata osd.traineddata)
        if(EXISTS "${IGI_TESSDATA_DIR}/${_tdata}")
            target_sources(igi PRIVATE "${IGI_TESSDATA_DIR}/${_tdata}")
            set_source_files_properties(
                "${IGI_TESSDATA_DIR}/${_tdata}"
                PROPERTIES
                    MACOSX_PACKAGE_LOCATION "Resources/tessdata"
            )
        endif()
    endforeach()

    # ── macdeployqt: bundle Qt dylibs + frameworks ──────────────────────────
    # SKIPPED in Debug builds — macdeployqt copies ~150 MB of Qt frameworks
    # and takes 2+ minutes, killing incremental rebuild speed.
    # Run `./bundle.sh --release` to get a fully deployable bundle.
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        NO_DEFAULT_PATH)

    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET igi POST_BUILD
            COMMAND "$<IF:$<CONFIG:Release>,${MACDEPLOYQT_EXECUTABLE},true>"
                    "$<IF:$<CONFIG:Release>,$<TARGET_BUNDLE_DIR:igi>,dummy>"
                    "$<IF:$<CONFIG:Release>,-always-overwrite,>"
            COMMENT "[igi] macdeployqt (Release only — skipped in Debug)…"
            VERBATIM
        )
    else()
        message(STATUS "[igi] macdeployqt not found — skipping Qt framework copy. "
                       "Run macdeployqt manually before distributing the bundle.")
    endif()
endif()

if(IGI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
