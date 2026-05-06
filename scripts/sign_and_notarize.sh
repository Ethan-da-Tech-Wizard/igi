#!/usr/bin/env bash
# scripts/sign_and_notarize.sh
#
# Sign Igi.app and optionally notarize it for Gatekeeper-clean distribution.
#
# ── Local dev (ad-hoc, no Apple Developer account) ───────────────────────────
#   ./scripts/sign_and_notarize.sh --adhoc
#
# ── CI / distribution (Developer ID cert required) ───────────────────────────
#   export APPLE_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAMID)"
#   export APPLE_ID="you@example.com"
#   export APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"   # App-specific password
#   export APPLE_TEAM_ID="YOURTEAMID"
#   ./scripts/sign_and_notarize.sh [--notarize]
#
# Environment variables:
#   APPLE_SIGNING_IDENTITY  Full name of the Developer ID cert in Keychain.
#                           Defaults to "-" (ad-hoc).
#   APPLE_ID                Apple ID email (notarization only).
#   APPLE_APP_PASSWORD      App-specific password from appleid.apple.com.
#   APPLE_TEAM_ID           10-char team ID shown in Apple Developer portal.
#   IGI_BUILD_DIR           Directory containing igi.app (default: ./build).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${IGI_BUILD_DIR:-${ROOT}/build}"
BUNDLE="${BUILD_DIR}/igi.app"
ENTITLEMENTS="${ROOT}/resources/Igi.entitlements"

SIGNING_IDENTITY="${APPLE_SIGNING_IDENTITY:--}"   # "-" = ad-hoc
DO_NOTARIZE=0
DO_ADHOC=0

for arg in "$@"; do
    case "$arg" in
        --adhoc)     DO_ADHOC=1; SIGNING_IDENTITY="-" ;;
        --notarize)  DO_NOTARIZE=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

if [[ ! -d "${BUNDLE}" ]]; then
    echo "ERROR: Bundle not found at ${BUNDLE}" >&2
    echo "       Run ./bundle.sh --release first." >&2
    exit 1
fi

# ── Step 1: Codesign ─────────────────────────────────────────────────────────
echo "==> Signing ${BUNDLE} with identity: '${SIGNING_IDENTITY}'"

# Sign all dylibs and frameworks inside the bundle first (inside-out rule).
find "${BUNDLE}/Contents/Frameworks" -name "*.dylib" -o -name "*.framework" \
  2>/dev/null | while read -r lib; do
    codesign --force --sign "${SIGNING_IDENTITY}" \
             --timestamp \
             "${lib}" 2>/dev/null || true
done

# Sign the main bundle.
codesign \
    --force \
    --deep \
    --sign "${SIGNING_IDENTITY}" \
    --entitlements "${ENTITLEMENTS}" \
    --options runtime \
    --timestamp \
    "${BUNDLE}"

echo "==> Codesign complete."
codesign --verify --deep --strict "${BUNDLE}" && echo "==> Verification: OK"

if [[ "${DO_ADHOC}" -eq 1 ]]; then
    echo "==> Ad-hoc signing complete. The app will run on this machine only."
    exit 0
fi

# ── Step 2: Create DMG for notarization ─────────────────────────────────────
DMG="${BUILD_DIR}/Igi.dmg"
if [[ "${DO_NOTARIZE}" -eq 1 ]]; then
    echo "==> Creating DMG for notarization…"
    hdiutil create -volname "Igi" \
        -srcfolder "${BUNDLE}" \
        -ov -format UDZO \
        "${DMG}"

    # ── Step 3: Notarize ─────────────────────────────────────────────────────
    echo "==> Submitting ${DMG} to Apple Notary Service…"

    : "${APPLE_ID:?APPLE_ID env var required for notarization}"
    : "${APPLE_APP_PASSWORD:?APPLE_APP_PASSWORD env var required}"
    : "${APPLE_TEAM_ID:?APPLE_TEAM_ID env var required}"

    xcrun notarytool submit "${DMG}" \
        --apple-id      "${APPLE_ID}" \
        --password      "${APPLE_APP_PASSWORD}" \
        --team-id       "${APPLE_TEAM_ID}" \
        --wait

    # ── Step 4: Staple ───────────────────────────────────────────────────────
    echo "==> Stapling notarization ticket to bundle…"
    xcrun stapler staple "${BUNDLE}"
    xcrun stapler validate "${BUNDLE}" && echo "==> Staple: OK"

    # Re-create the DMG with the stapled bundle.
    hdiutil create -volname "Igi" \
        -srcfolder "${BUNDLE}" \
        -ov -format UDZO \
        "${DMG}"
    echo "==> Notarized DMG ready: ${DMG}"
fi
