#!/usr/bin/env python3
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

SCRIPT_DIR = pathlib.Path(__file__).parent
LOCK_FILE  = SCRIPT_DIR.parent / "deps.lock"

PACKAGES = ["tesseract", "leptonica", "qt"]


def brew_prefix(pkg: str) -> str:
    try:
        r = subprocess.run(["brew", "--prefix", pkg],
                           capture_output=True, text=True, timeout=15)
        return r.stdout.strip() if r.returncode == 0 else ""
    except Exception:
        return ""


def brew_version(pkg: str) -> str:
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
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def find_dylib(prefix: str, pkg: str) -> pathlib.Path | None:
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

    failures = 0
    print("\n==> Verifying against lockfile…")

    for pkg in PACKAGES:
        if pkg not in lock:
            print(f"WARN  [{pkg}] Not in lockfile — skipping.")
            continue

        expected = lock[pkg]
        actual   = current.get(pkg, {})

        # Layer 1: version.
        if actual.get("version") != expected["version"]:
            print(f"FAIL  [{pkg}] Version: expected={expected['version']} "
                  f"actual={actual.get('version', 'N/A')}", file=sys.stderr)
            failures += 1
        else:
            print(f"PASS  [{pkg}] Version: {actual['version']}")

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
