# Igi — Visual Search

A native visual-search overlay that applies OCR and fuzzy matching to screen pixels and scanned documents.

**Status:** Desktop search prototype  
**Built with:** C++17, Qt 6, Tesseract, Leptonica, optional Poppler

## Reading order

1. Read the root README for project purpose and setup navigation.
2. Follow the entry points below to find the implementation.
3. Use the file guide to locate individual modules, tests, assets, and configuration.
4. Check component licenses before redistributing dependencies or assets.

## Important paths

- [CMakeLists.txt](../CMakeLists.txt) — CMake build definition for .; declares targets, dependency discovery, and compiler/build settings.
- [src](../src) — Application source and platform integration
- [docs/DECISIONS.md](../docs/DECISIONS.md) — Documentation: Igi: Engineering Decisions Log. Covers D-001 — Fuzzy match scoring formula; D-002 — End-to-end latency SLOs; D-003 — Memory budgets.

## Structure

| Path | Role |
| --- | --- |
| [.github/](../.github/) | GitHub workflow and contributor configuration (1 tracked entries). |
| [assets/](../assets/) | Media and visual resources used by the project (1 tracked entries). |
| [docs/](../docs/) | Project documentation, decisions, and reference material (21 tracked entries). |
| [igi_linux_win/](../igi_linux_win/) | Project-specific igi linux win components (42 tracked entries). |
| [resources/](../resources/) | Project-specific resources components (7 tracked entries). |
| [scripts/](../scripts/) | Build, launch, maintenance, and verification commands (4 tracked entries). |
| [src/](../src/) | Application source and platform integration (41 tracked entries). |
| [tests/](../tests/) | Automated checks and test fixtures (12 tracked entries). |

## Useful commands

Run from the repository root after following the documented prerequisites.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Scope

Install the platform dependencies in README.md first. Poppler introduces GPL obligations when included in a distributed build; the project MIT license does not override that dependency.

## Maintaining the guide

The file catalog is a source-inspection snapshot, not proof that every runtime path works. Update it when moving files or changing module responsibilities. The 2026-10-12 documentation review did not run every application, deploy services, or validate all historical performance claims.
