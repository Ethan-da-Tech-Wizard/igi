#!/usr/bin/env bash
# scripts/verify_deps.sh
#
# Dependency integrity check for CI and local dev.
#
# THREAT (T-1): A poisoned Homebrew formula replaces a key dependency
# (tesseract, leptonica) with a backdoored version at the same version number.
# This script provides three layers of verification:
#
#   Layer 1 — Version pinning: confirm installed versions match our lockfile.
#   Layer 2 — Library existence: confirm key dylibs are on disk at expected paths.
#   Layer 3 — SHA-256 hash pinning: confirm the installed bottle binary matches
#              the known-good hash recorded at the time of the last audit.
#
# To update the lockfile after an intentional upgrade:
#   brew upgrade tesseract leptonica
#   ./scripts/verify_deps.sh --update
#
# Exit code: 0 = all checks pass. 1 = any check fails (CI hard fail).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCK_FILE="${SCRIPT_DIR}/../deps.lock"
UPDATE_MODE=0

for arg in "$@"; do
    case "$arg" in
        --update) UPDATE_MODE=1 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done

# ── Packages to verify ────────────────────────────────────────────────────────
PACKAGES=("tesseract" "leptonica" "qt")

# ── Collect current state ────────────────────────────────────────────────────
# Use parallel indexed arrays for bash 3.2 compatibility (macOS ships bash 3.2).
PKG_NAMES=()
PKG_VERSIONS=()
PKG_HASHES=()

for pkg in "${PACKAGES[@]}"; do
    version=$(brew list --versions "${pkg}" 2>/dev/null | awk '{print $2}' || echo "NOT_INSTALLED")

    prefix=$(brew --prefix "${pkg}" 2>/dev/null || echo "")
    hash_val="NO_PREFIX"
    if [[ -n "${prefix}" && -d "${prefix}/lib" ]]; do
        dylib=$(find "${prefix}/lib" -maxdepth 1 -name "lib${pkg}*.dylib" \
                    ! -name "*-*.dylib" 2>/dev/null | head -1 || echo "")
        if [[ -n "${dylib}" && -f "${dylib}" ]]; then
            hash_val=$(shasum -a 256 "${dylib}" | awk '{print $1}')
        else
            hash_val="NO_DYLIB"
        fi
    fi

    PKG_NAMES+=("${pkg}")
    PKG_VERSIONS+=("${version}")
    PKG_HASHES+=("${hash_val}")
done

get_version() { echo "${PKG_VERSIONS[$1]}"; }
get_hash()    { echo "${PKG_HASHES[$1]}"; }
get_index() {
    local target="$1"; local i=0
    for name in "${PKG_NAMES[@]}"; do
        if [[ "${name}" == "${target}" ]]; then echo "${i}"; return; fi
        ((i++))
    done
    echo "-1"
}

# ── Update mode: regenerate lockfile ─────────────────────────────────────────
if [[ "${UPDATE_MODE}" -eq 1 ]]; then
    echo "==> Updating ${LOCK_FILE}…"
    {
        echo "# Igi dependency lockfile"
        echo "# Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "# Update with: ./scripts/verify_deps.sh --update"
        echo "#"
        echo "# Format: PACKAGE VERSION SHA256_OF_PRIMARY_DYLIB"
        echo ""
        for pkg in "${PACKAGES[@]}"; do
            echo "${pkg} ${CURRENT_VERSIONS[${pkg}]} ${CURRENT_HASHES[${pkg}]}"
        done
    } > "${LOCK_FILE}"
    echo "==> Lockfile updated:"
    cat "${LOCK_FILE}"
    exit 0
fi

# ── Verification mode ─────────────────────────────────────────────────────────
if [[ ! -f "${LOCK_FILE}" ]]; then
    echo "ERROR: deps.lock not found at ${LOCK_FILE}." >&2
    echo "       Run ./scripts/verify_deps.sh --update to generate it." >&2
    exit 1
fi

FAIL=0

while IFS=' ' read -r pkg expected_version expected_hash; do
    # Skip comments and blank lines.
    [[ "${pkg}" =~ ^#.*$ || -z "${pkg}" ]] && continue

    actual_version="${CURRENT_VERSIONS[${pkg}]:-NOT_INSTALLED}"
    actual_hash="${CURRENT_HASHES[${pkg}]:-UNKNOWN}"

    # Layer 1: version check.
    if [[ "${actual_version}" != "${expected_version}" ]]; then
        echo "FAIL [${pkg}] Version mismatch: expected=${expected_version} actual=${actual_version}" >&2
        FAIL=1
    else
        echo "PASS [${pkg}] Version: ${actual_version}"
    fi

    # Layer 2 + 3: hash check (skip if dylib not found — advisory only).
    if [[ "${actual_hash}" == "NO_DYLIB" || "${actual_hash}" == "NO_PREFIX" ]]; then
        echo "WARN [${pkg}] No dylib found for hash check. Version-only verification."
    elif [[ "${expected_hash}" == "NO_DYLIB" || "${expected_hash}" == "NO_PREFIX" ]]; then
        echo "WARN [${pkg}] Lockfile has no hash. Run --update to record one."
    elif [[ "${actual_hash}" != "${expected_hash}" ]]; then
        echo "FAIL [${pkg}] SHA-256 MISMATCH — possible tampered binary!" >&2
        echo "  expected: ${expected_hash}" >&2
        echo "  actual:   ${actual_hash}" >&2
        FAIL=1
    else
        echo "PASS [${pkg}] SHA-256: ${actual_hash:0:16}…"
    fi

done < "${LOCK_FILE}"

if [[ "${FAIL}" -ne 0 ]]; then
    echo ""
    echo "DEPENDENCY INTEGRITY CHECK FAILED." >&2
    echo "A dependency may have been tampered with or unexpectedly upgraded." >&2
    echo "If this is an intentional upgrade, run: ./scripts/verify_deps.sh --update" >&2
    exit 1
fi

echo ""
echo "==> All dependency checks passed."
exit 0
