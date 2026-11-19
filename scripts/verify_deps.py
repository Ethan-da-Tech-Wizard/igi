#!/usr/bin/env python3
# ==============================================================================
# Program: verify_deps.py
# Programmer: Ethan-da-Tech-Wizard
# Last Modified: November 18, 2026
# Purpose: Verifies dependency binary integrity and dynamic library SHA-256 cryptographic hashes against a locked baseline manifest to protect against supply-chain tampering.
# ==============================================================================

"""
scripts/verify_deps.sh — Dependency integrity check (Python 3, no bash 4 required).

THREAT (T-1): A poisoned Homebrew formula replaces tesseract/leptonica with a
backdoored version. This script verifies installed versions and SHA-256 hashes
of the primary dylibs against a committed lockfile.

Usage:
  python3 scripts/verify_deps.py          # verify (CI mode)
  python3 scripts/verify_deps.py --update # regenerate lockfile after upgrade

Exit: 0 = pass, 1 = fail.
"""

import sys
import hashlib
import subprocess
import pathlib
import json
import datetime

# Where we are and where the lockfile lives: this file is in scripts/, so the
# lockfile deps.lock sits one level up (repository root) whatever the caller's cwd.
SCRIPT_DIR = pathlib.Path(__file__).parent
LOCK_FILE  = SCRIPT_DIR.parent / "deps.lock"

# Homebrew formulae whose installed state is pinned. The first two are the OCR
# libraries linked into the app (threat T-1); qt is recorded for its version only
# (its library name does not match libqt*.dylib, so the lock holds NO_DYLIB).
PACKAGES = ["tesseract", "leptonica", "qt"]


def brew_prefix(pkg: str) -> str:
    """Returns `brew --prefix <pkg>` (the install directory).

    Args:
        pkg: Homebrew formula name.

    Returns:
        The prefix path, or "" if brew fails or times out (15 s). Errors are
        swallowed on purpose: the caller treats "" as "not found".
    """
    try:
        r = subprocess.run(["brew", "--prefix", pkg],
                           capture_output=True, text=True, timeout=15)
        return r.stdout.strip() if r.returncode == 0 else ""
    except Exception:
        return ""


def brew_version(pkg: str) -> str:
    """Returns the installed version of a formula.

    Args:
        pkg: Homebrew formula name.

    Returns:
        The second word of `brew list --versions <pkg>`, "UNKNOWN" if the line has
        no version, or "NOT_INSTALLED" if brew fails or lists nothing.
    """
    try:
        r = subprocess.run(["brew", "list", "--versions", pkg],
                           capture_output=True, text=True, timeout=15)
        if r.returncode == 0 and r.stdout.strip():
            parts = r.stdout.strip().split()
            return parts[1] if len(parts) >= 2 else "UNKNOWN"
        return "NOT_INSTALLED"
    except Exception:
        return "NOT_INSTALLED"


def sha256_file(path: pathlib.Path) -> str:
    """Returns the hex SHA-256 of a file, read in 64 KiB chunks so large dylibs never have to fit in memory."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def find_dylib(prefix: str, pkg: str) -> pathlib.Path | None:
    """Locates a package's primary library under <prefix>/lib.

    The glob lib<pkg>*.dylib can match several files (versioned names and
    symlinks); the first real file (not a symlink) in sorted order is used.

    Returns:
        The path, or None if the lib directory or a matching file is missing.
    """
    lib_dir = pathlib.Path(prefix) / "lib"
    if not lib_dir.is_dir():
        return None
    # Match libPKG*.dylib but not libPKG-something (versioned symlinks).
    candidates = sorted(lib_dir.glob(f"lib{pkg}*.dylib"))
    # Prefer the non-symlink real file.
    for c in candidates:
        if not c.is_symlink():
            return c
    return candidates[0] if candidates else None


def collect() -> dict:
    """Gathers {package: {version, sha256, dylib}} for every pinned package as the machine is right now.

    "NO_DYLIB" is recorded when no library is found, so the lockfile written by
    --update and a later check agree about it.
    """
    result = {}
    for pkg in PACKAGES:
        version = brew_version(pkg)
        prefix  = brew_prefix(pkg)
        dylib   = find_dylib(prefix, pkg) if prefix else None
        sha     = sha256_file(dylib) if dylib else "NO_DYLIB"
        result[pkg] = {"version": version, "sha256": sha,
                       "dylib": str(dylib) if dylib else ""}
    return result


def load_lock() -> dict:
    """Parses deps.lock into {package: {version, sha256}}.

    Blank lines and lines starting with "#" are skipped; each remaining line is
    "PACKAGE  VERSION  SHA256" separated by whitespace. A missing file gives {},
    which main() reports as an error.
    """
    if not LOCK_FILE.exists():
        return {}
    data = {}
    for line in LOCK_FILE.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 3:
            data[parts[0]] = {"version": parts[1], "sha256": parts[2]}
    return data


def save_lock(state: dict) -> None:
    """Rewrites deps.lock from the given state with a header (timestamp, usage, format).

    Only used by `--update`, after a deliberate dependency upgrade.
    """
    lines = [
        "# Igi dependency lockfile",
        f"# Generated: {datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ')}",
        "# Update with: python3 scripts/verify_deps.py --update",
        "#",
        "# FORMAT: PACKAGE  VERSION  SHA256_OF_PRIMARY_DYLIB",
        "",
    ]
    for pkg, info in state.items():
        lines.append(f"{pkg}  {info['version']}  {info['sha256']}")
    LOCK_FILE.write_text("\n".join(lines) + "\n")
    print(f"==> Lockfile written to {LOCK_FILE}")
    print(LOCK_FILE.read_text())


def main() -> int:
    """Entry point.

    Without arguments: collect the installed state and compare every pinned
    package with the lockfile (layer 1 version, layer 2/3 library SHA-256). Any
    mismatch makes the exit code 1, which fails the CI step "Verify dependency
    integrity". With --update: regenerate the lockfile and exit 0.

    Returns:
        0 if everything matches (or after --update), 1 otherwise.
    """
    update_mode = "--update" in sys.argv

    print("==> Collecting installed dependency state…")
    current = collect()
    for pkg, info in current.items():
        print(f"    {pkg}: {info['version']}  dylib={info['dylib'] or 'not found'}")

    if update_mode:
        save_lock(current)
        return 0

    lock = load_lock()
    if not lock:
        print(f"ERROR: {LOCK_FILE} not found or empty.", file=sys.stderr)
        print("       Run: python3 scripts/verify_deps.py --update", file=sys.stderr)
        return 1

        # Counts mismatches so every problem is printed before the script exits non-zero.
    failures = 0
    print("\n==> Verifying against lockfile…")

    for pkg in PACKAGES:
        if pkg not in lock:
            print(f"WARN  [{pkg}] Not in lockfile — skipping.")
            continue

        expected = lock[pkg]
        actual   = current.get(pkg, {})

                # (Layer 1) The Homebrew version string must match the lock exactly; a
                # silently upgraded or replaced formula changes it.
        # Layer 1: version.
        if actual.get("version") != expected["version"]:
            print(f"FAIL  [{pkg}] Version: expected={expected['version']} "
                  f"actual={actual.get('version', 'N/A')}", file=sys.stderr)
            failures += 1
        else:
            print(f"PASS  [{pkg}] Version: {actual['version']}")

                # (Layers 2-3) The hash of the primary library must match: detects a
                # tampered binary even when the version string is unchanged. Missing
                # libraries or hashes only warn, they do not fail.
        # Layer 2+3: SHA-256.
        exp_sha = expected.get("sha256", "")
        act_sha = actual.get("sha256", "")

        if act_sha in ("NO_DYLIB", ""):
            print(f"WARN  [{pkg}] No dylib found — skipping hash check.")
        elif exp_sha in ("NO_DYLIB", ""):
            print(f"WARN  [{pkg}] No hash in lockfile — run --update.")
        elif act_sha != exp_sha:
            print(f"FAIL  [{pkg}] SHA-256 MISMATCH — possible tampered binary!",
                  file=sys.stderr)
            print(f"        expected: {exp_sha}", file=sys.stderr)
            print(f"        actual:   {act_sha}", file=sys.stderr)
            failures += 1
        else:
            print(f"PASS  [{pkg}] SHA-256: {act_sha[:16]}…")

    print()
    if failures:
        print(f"DEPENDENCY INTEGRITY CHECK FAILED ({failures} failure(s)).",
              file=sys.stderr)
        print("A dependency may have been tampered with or upgraded unexpectedly.",
              file=sys.stderr)
        print("If intentional: python3 scripts/verify_deps.py --update",
              file=sys.stderr)
        return 1

    print("==> All dependency checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
