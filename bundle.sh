#!/usr/bin/env bash
# Program: igi - bundle.sh
# Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
# Last Modified: 2026-11-18
# Purpose: Builds the app bundle with CMake and optionally packages it into a disk image.

# bundle.sh — Build Igi.app and optionally package it as a DMG.
#
# Usage:
#   ./bundle.sh              # Debug build, no DMG
#   ./bundle.sh --release    # Release build, no DMG
#   ./bundle.sh --dmg        # Release build + create Igi.dmg
#
# The resulting bundle lives at:  build/igi.app
# The DMG (if requested) lives at: build/Igi.dmg
#
# Requires: cmake, Xcode CLI tools, Homebrew Qt, Tesseract, Leptonica.

set -euo pipefail

# Directory containing this script, so it works from any current directory.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
BUILD_TYPE="Debug"
MAKE_DMG=0

for arg in "$@"; do
    case "$arg" in
        --release) BUILD_TYPE="Release" ;;
        --dmg)     BUILD_TYPE="Release"; MAKE_DMG=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

# Configure the CMake build (tests are turned off for bundles).
echo "==> Configuring (${BUILD_TYPE})…"
cmake -B "${BUILD_DIR}" -S "${ROOT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DIGI_BUILD_TESTS=OFF

# Build only the igi application target.
echo "==> Building…"
cmake --build "${BUILD_DIR}" --target igi --parallel

BUNDLE="${BUILD_DIR}/igi.app"
echo "==> Bundle: ${BUNDLE}"

# Package the bundle into a compressed disk image when --dmg was requested.
if [[ "${MAKE_DMG}" -eq 1 ]]; then
    DMG="${BUILD_DIR}/Igi.dmg"
    echo "==> Creating DMG at ${DMG}…"
    hdiutil create -volname "Igi" \
        -srcfolder "${BUNDLE}" \
        -ov -format UDZO \
        "${DMG}"
    echo "==> Done: ${DMG}"
else
    echo "==> Done. Run with: open ${BUNDLE}"
fi
