# Igi — Visual Search

A native visual-search overlay that applies OCR and fuzzy matching to screen pixels and scanned documents.

**Status:** Desktop search prototype  
**Built with:** C++17, Qt 6, Tesseract, Leptonica, optional Poppler

[Project guide](docs/PROJECT_GUIDE.md) · [Every file explained](docs/FILE_GUIDE.md) · [Licenses and dependencies](docs/LICENSE_REVIEW.md)

## Start here

- [CMakeLists.txt](CMakeLists.txt) — CMake build definition for .; declares targets, dependency discovery, and compiler/build settings.
- [src](src) — Application source and platform integration
- [docs/DECISIONS.md](docs/DECISIONS.md) — Documentation: Igi: Engineering Decisions Log. Covers D-001 — Fuzzy match scoring formula; D-002 — End-to-end latency SLOs; D-003 — Memory budgets.

The following commands are taken from this project’s source/configuration. Install its prerequisites first.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

[Detailed setup, usage, and development notes](PROJECT_NOTES.md) preserve the previous project documentation.

## Repository layout

| Path | Role |
| --- | --- |
| [.github/](.github/) | GitHub workflow and contributor configuration (1 tracked entries). |
| [assets/](assets/) | Media and visual resources used by the project (1 tracked entries). |
| [docs/](docs/) | Project documentation, decisions, and reference material (21 tracked entries). |
| [igi_linux_win/](igi_linux_win/) | Project-specific igi linux win components (42 tracked entries). |
| [resources/](resources/) | Project-specific resources components (7 tracked entries). |
| [scripts/](scripts/) | Build, launch, maintenance, and verification commands (4 tracked entries). |
| [src/](src/) | Application source and platform integration (41 tracked entries). |
| [tests/](tests/) | Automated checks and test fixtures (12 tracked entries). |

## Scope and current limits

Install the platform dependencies in README.md first. Poppler introduces GPL obligations when included in a distributed build; the project MIT license does not override that dependency.

## License

Original project code retains **MIT** terms in [LICENSE](LICENSE). Third-party libraries, media, data, models, and tools retain their own terms. See the [license review](docs/LICENSE_REVIEW.md) and [dependency inventory](docs/DEPENDENCIES.csv).
