#!/usr/bin/env bash
# scripts/sandbox_test.sh
#
# D-007 write-deny sandbox test (DECISIONS.md D-007).
#
# Verifies that the igi binary emits ZERO filesystem writes during a
# clean startup-and-shutdown cycle. Uses macOS sandbox-exec with the
# igi.sb profile which denies all writes except a temporary scratch dir.
#
# Exit codes:
#   0 — No write violations detected (D-007 passes).
#   1 — Write violation detected or binary crashed unexpectedly.
#
# Usage (CI):
#   ./scripts/sandbox_test.sh ./build/igi.app/Contents/MacOS/igi
#
# Usage (local):
#   ./scripts/sandbox_test.sh   # Uses ./build/igi.app/Contents/MacOS/igi

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${1:-${ROOT}/build/igi.app/Contents/MacOS/igi}"
PROFILE="${ROOT}/resources/igi.sb"

if [[ ! -x "${BINARY}" ]]; then
    echo "ERROR: igi binary not found or not executable at: ${BINARY}" >&2
    echo "       Build with ./bundle.sh first." >&2
    exit 1
fi

if [[ ! -f "${PROFILE}" ]]; then
    echo "ERROR: Sandbox profile not found at: ${PROFILE}" >&2
    exit 1
fi

# Temp dir that the sandbox allows writes to.
SCRATCH="$(mktemp -d)"
trap 'rm -rf "${SCRATCH}"' EXIT

echo "==> D-007 write-deny sandbox test"
echo "    Binary:  ${BINARY}"
echo "    Profile: ${PROFILE}"
echo "    Scratch: ${SCRATCH}"

# Run igi for 2 seconds then send SIGTERM.
# The --smoke flag (not yet implemented) will be wired in a later patch;
# for now we rely on timeout to stop the daemon cleanly.
#
# sandbox-exec passes SCRATCH_DIR as a parameter into the .sb profile
# via the (param "SCRATCH_DIR") directive.
VIOLATION_LOG="${SCRATCH}/sandbox.log"

set +e
sandbox-exec \
    -f "${PROFILE}" \
    -D "SCRATCH_DIR=${SCRATCH}" \
    /usr/bin/timeout 3 "${BINARY}" \
    > "${SCRATCH}/igi_stdout.log" \
    2> "${SCRATCH}/igi_stderr.log"
EXIT_CODE=$?
set -e

# timeout returns 124 on timeout (normal), 0 on clean exit.
# Any other exit code from sandbox-exec may indicate a crash or violation.
if [[ "${EXIT_CODE}" -eq 124 || "${EXIT_CODE}" -eq 0 || "${EXIT_CODE}" -eq 143 ]]; then
    echo "==> igi exited cleanly under sandbox (exit ${EXIT_CODE})."
else
    echo "FAIL: igi crashed or was killed by the sandbox (exit ${EXIT_CODE})." >&2
    echo "--- stdout ---"
    cat "${SCRATCH}/igi_stdout.log" >&2
    echo "--- stderr ---"
    cat "${SCRATCH}/igi_stderr.log" >&2
    exit 1
fi

# Check sandbox log for any DENY entries (write violations).
# macOS logs sandbox violations to the system log; we capture them via log(1).
#
# We look back 5 seconds for any sandbox deny event attributed to our PID.
# This is best-effort; a future patch can use `sandbox_check()` in-process.
echo "==> Checking system log for sandbox write violations…"
sleep 1   # give syslog time to flush

VIOLATIONS=$(log show \
    --predicate 'subsystem == "com.apple.sandbox" AND eventMessage CONTAINS "deny"' \
    --last 10s \
    --style syslog \
    2>/dev/null | grep -i "igi" || true)

if [[ -n "${VIOLATIONS}" ]]; then
    echo "FAIL: Sandbox write violation(s) detected:" >&2
    echo "${VIOLATIONS}" >&2
    exit 1
fi

echo "==> D-007 PASS: No write violations detected."
exit 0
