# Igi — Visual Search

Igi is a desktop visual-search prototype. Its source combines screen capture, OCR, and fuzzy text matching to explore searching text shown on screen.

**Status:** Prototype. Platform support, accuracy, performance, and release packaging have not been independently verified in this review. Historical project notes contain ambitious claims that require separate validation.

## Build and test

The CMake project uses C++17, Qt 6, Tesseract and Leptonica; Poppler is optional for PDF ingestion. Install the dependencies appropriate for your platform, then from the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The commands reflect the project configuration; they were not run as part of this review. Platform capture permissions and desktop integration can affect operation. Consult [PROJECT_NOTES.md](PROJECT_NOTES.md) and [engineering decisions](docs/DECISIONS.md), checking historical release, privacy and performance claims before relying on them.

## Explore

[Source](src/) · [Tests](tests/) · [Project guide](docs/PROJECT_GUIDE.md) · [License review](docs/LICENSE_REVIEW.md)

## License

Project code is distributed under the [MIT License](LICENSE). Tesseract, Leptonica, Qt, optional Poppler and other dependencies have separate terms. In particular, review the licensing implications of distributing Poppler with a build in the [license review](docs/LICENSE_REVIEW.md).
