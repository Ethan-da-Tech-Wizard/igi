# Igi: Engineering Decisions Log

A living record of binding technical decisions made during Igi's development.
Each entry has a date, a status (Accepted / Superseded), and a one-line
rationale. Decisions are referenced by ID (e.g., `D-001`) from code comments
and PRs.

---

## D-001 — Fuzzy match scoring formula
**Status:** Accepted (Chunk 0)

We rank OCR-extracted words against the user's query using a normalized
Levenshtein distance:

```
score = 1.0 - levenshtein(query, candidate) / max(len(query), len(candidate))
```

- `score ∈ [0.0, 1.0]`, higher is better.
- Default match threshold: `0.85` (configurable per call).
- Both strings are lowercased and stripped of punctuation before scoring.
- Implementation: Wagner-Fischer with row-rolling and early-exit when the
  running minimum exceeds `(1 - threshold) * max(len)`.

**Why this formula:** It produces a stable percentage that is intuitive to
tune, handles short and long words symmetrically, and matches the ">90%
confidence" language in `FRD.md` (which previously had no operational
definition).

---

## D-002 — End-to-end latency SLOs
**Status:** Accepted (Chunk 0)

Measured from hotkey press to visible highlight on a single 4K screen
capture, on Apple Silicon (M1/M2/M3):

| Percentile | Budget |
|------------|--------|
| p50        | 800 ms |
| p95        | 1500 ms |
| p99        | 2500 ms |

Sub-budgets (informational, not enforced individually):

- Capture + DPR conversion: < 100 ms
- QImage → PIX conversion: < 50 ms
- Tesseract `Recognize()` (single screen): < 600 ms
- Fuzzy search top-K over 5000 words: < 16 ms
- Highlight render: < 16 ms (one frame at 60 Hz)

**Why these numbers:** The README/PRD previously claimed "milliseconds" and
"instantaneous", which Tesseract cannot deliver on a 5K capture. These SLOs
are honest, verifiable in CI, and still feel "instant" to a user.

---

## D-003 — Memory budgets
**Status:** Accepted (Chunk 0)

| Workload                    | Soft cap | Hard cap |
|-----------------------------|---------:|---------:|
| Idle daemon                 |    50 MB |   100 MB |
| Active session, 4K capture  |   250 MB |   400 MB |
| Active session, 50-page PDF |   500 MB |  1024 MB |

- Soft cap = target; we treat exceeding it as a regression.
- Hard cap = release blocker; CI fails the build.
- Post-session RSS must return within +5 MB of pre-session baseline (proves
  teardown actually frees).

---

## D-004 — Primary global hotkey API: NSEvent, not Carbon
**Status:** Accepted (Chunk 0)

We register the global hotkey via
`+[NSEvent addGlobalMonitorForEventsMatchingMask:handler:]` as the primary
path. `Carbon.framework`'s `RegisterEventHotKey` is retained as a compiled
fallback only.

**Why:** Carbon is soft-deprecated and tracked as ENG-01 in
`RISK_REGISTER.md`. Leading with `NSEvent` removes the risk preemptively.
Both implementations sit behind a shared `IHotkeyListener` interface, so the
fallback can be selected at startup if `NSEvent` ever fails on a future
macOS release.

---

## D-005 — Test framework: GoogleTest
**Status:** Accepted (Chunk 0)

Pulled in via CMake `FetchContent` at version `v1.14.0`. Qt Test was
considered but rejected:

- The bulk of Igi's logic (fuzzy search, image conversion, OCR plumbing) is
  Qt-free; GoogleTest is more idiomatic for that code.
- `QSignalSpy` is independently usable from within GoogleTest assertions, so
  we lose nothing on the Qt side.
- GoogleTest's mocking story (gMock) is materially better for the
  `IHotkeyListener` and other interfaces we will introduce in later chunks.

---

## D-006 — Secure memory wipe API
**Status:** Accepted (Chunk 0)

All sensitive byte buffers (PIX data, QPixmap pixels, OCR'd word strings)
must be zeroed using `explicit_bzero(ptr, len)` (or a `volatile` overwrite
loop on platforms that lack it) before deallocation.

`memset(ptr, 0, len)` is **not** acceptable — modern compilers elide
"dead store" zeroing of soon-to-be-freed memory. This corrects the
mitigation listed in `RISK_REGISTER.md` SEC-02.

---

## D-007 — Disk reads vs. writes
**Status:** Accepted (Chunk 0)

The "zero-storage" guarantee applies to **writes**, not reads. Specifically:

- **Allowed:** `open()` / `mmap()` / `fz_open_document()` against existing
  user files (e.g., a PDF the user already opened in Preview), reading
  Tesseract `tessdata` from the app bundle, reading Qt resource files.
- **Prohibited:** Any `write()`, `fwrite()`, `pwrite()`, `mkstemp()`, log
  files, screenshots, caches, or analytics payloads emitted by Igi.

The Chunk 7 release sandbox test enforces this distinction by running the
binary under a `sandbox-exec` profile that denies all writes outside an
allowlisted test directory.
