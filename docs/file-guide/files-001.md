# Files 1 of 1 — Igi — Visual Search

[All pages](../FILE_GUIDE.md) · [Project guide](../PROJECT_GUIDE.md)

| File | Role and useful details |
| --- | --- |
| [.github/workflows/ci.yml](../../.github/workflows/ci.yml) | GitHub Actions workflow for ci; defines automation triggers, jobs, and build/check steps. |
| [.gitignore](../../.gitignore) | Defines local data, credentials, dependencies, and generated outputs that Git should leave untracked. |
| [CMakeLists.txt](../../CMakeLists.txt) | CMake build definition for .; declares targets, dependency discovery, and compiler/build settings. |
| [CMakePresets.json](../../CMakePresets.json) | Structured CMakePresets data; top-level fields: `version`, `configurePresets`. |
| [LICENSE](../../LICENSE) | Preserved license, copyright, or attribution terms; read the full text before reusing the covered component. |
| [PROJECT_NOTES.md](../../PROJECT_NOTES.md) | Preserved detailed setup, usage, architecture, and development notes from the earlier README. |
| [README.md](../../README.md) | Project overview, current status, entry points, setup navigation, and documentation links. |
| [assets/.gitkeep](../../assets/.gitkeep) | Tracked .gitkeep resource in assets; 135 bytes. |
| [bundle.sh](../../bundle.sh) | Command-line automation for bundle; review prerequisites and side effects before executing. |
| [deps.lock](../../deps.lock) | Tracked deps resource in .; 358 bytes. |
| [docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md) | Documentation: Igi: Core Systems Architecture. Covers 1. High-Level Component Architecture; 2. The Microsecond Execution Sequence; 3. Memory & Security Isolation Layer. |
| [docs/BUILDING.md](../../docs/BUILDING.md) | Documentation: Building Igi OCR from Source. Covers 1. 🏔 Arch Linux / Manjaro; Prerequisites; Build & Run Tests. |
| [docs/CHUNKSTONES](../../docs/CHUNKSTONES) | Tracked CHUNKSTONES resource in docs; 10,534 bytes. |
| [docs/CROSS_PLATFORM_PLAN.md](../../docs/CROSS_PLATFORM_PLAN.md) | Documentation: Igi: Cross-Platform Compatibility Plan. Covers 1. Where we start (research summary); 1.1 What is already cross-platform (keep as-is, zero changes); 1.2 What is macOS-only today (keep, but add siblings). |
| [docs/CROSS_PLATFORM_TODO.md](../../docs/CROSS_PLATFORM_TODO.md) | Documentation: Igi: Cross-Platform TODO / Plan of Action. Covers Phase 0 — Portability groundwork (no user-visible features); Phase 1 — Linux backend, X11 first; Phase 2 — Linux Wayland (portals + graceful degradation). |
| [docs/DECISIONS.md](../../docs/DECISIONS.md) | Documentation: Igi: Engineering Decisions Log. Covers D-001 — Fuzzy match scoring formula; D-002 — End-to-end latency SLOs; D-003 — Memory budgets. |
| [docs/DEPENDENCIES.csv](../../docs/DEPENDENCIES.csv) | Machine-readable inventory of declared and locked dependencies, versions, reported licenses, and evidence URLs. |
| [docs/FILE_GUIDE.md](../../docs/FILE_GUIDE.md) | Index of the complete tracked-file guide, with one entry for every file and links to the paginated descriptions. |
| [docs/FRD.md](../../docs/FRD.md) | Documentation: Functional Requirements Document (FRD): Igi. Covers 1. System Initialization & Lifecycle; 2. Triggering & Visual Capture; 3. OCR Processing Pipeline. |
| [docs/LICENSE_REVIEW.md](../../docs/LICENSE_REVIEW.md) | Original-code license, preserved component notices, dependency-license evidence, and unresolved distribution questions. |
| [docs/MDM_DEPLOYMENT.md](../../docs/MDM_DEPLOYMENT.md) | Documentation: Igi OCR Search Engine — MDM Deployment Guide. Covers 1. PPPC (Privacy Preferences Policy Control) Profile; Igi Bundle Identifier & Code Requirement; Required PPPC Grants. |
| [docs/PIPELINE_DIAGRAMS.md](../../docs/PIPELINE_DIAGRAMS.md) | Documentation: 🖼 Igi Visual Pipelines. Covers 1. System UI & UX Concept; 2. High-Level Offline Architecture Pipeline; Strict Logic Flow (Engineering Standard). |
| [docs/PRD.md](../../docs/PRD.md) | Documentation: Product Requirements Document (PRD): Igi. Covers 1. Product Overview; 2. Core Features & Requirements; 3. Technology Stack. |
| [docs/PROBLEM_STATEMENT.md](../../docs/PROBLEM_STATEMENT.md) | Documentation: Igi: Problem Statement & Scope Definition. Covers The Problem; The Solution. |
| [docs/PROJECT_GUIDE.md](../../docs/PROJECT_GUIDE.md) | Maintainer orientation: purpose, important entry points, source layout, commands, and project-specific boundaries. |
| [docs/REPOSITORY_STRUCTURE.md](../../docs/REPOSITORY_STRUCTURE.md) | Documentation: Igi: Repository Structure Plan. Covers Directory Tree; Architectural Rules. |
| [docs/RISK_REGISTER.md](../../docs/RISK_REGISTER.md) | Documentation: Igi: Comprehensive Risk Register. Covers 1. Security & Privacy Risks; 2. Performance & Memory Risks; 3. Engineering & Compatibility Risks. |
| [docs/SCOPE_AND_MILESTONES.md](../../docs/SCOPE_AND_MILESTONES.md) | Documentation: Igi: Scope Lock & Milestones. Covers 1. Scope Lock (The Boundaries); 🟢 In-Scope (Strictly Required); 🔴 Out-of-Scope (Strictly Prohibited). |
| [docs/SRD.md](../../docs/SRD.md) | Documentation: System Requirements Document (SRD): Igi. Covers 1. Operating System Specifications; 2. Hardware Constraints; 3. Development & Build Environment. |
| [docs/THREAT_MODEL.md](../../docs/THREAT_MODEL.md) | Documentation: Igi: Threat Model & Attack Surface Analysis. Covers Threat 1 — Process Injection into Igi; What the threat is; Cyber vector. |
| [docs/file-guide/files-001.md](../../docs/file-guide/files-001.md) | One page of the tracked-file catalog; links each listed path to its role and source-derived details. |
| [igi_linux_win/CMakeLists.txt](../../igi_linux_win/CMakeLists.txt) | CMake build definition for igi_linux_win; declares targets, dependency discovery, and compiler/build settings. |
| [igi_linux_win/src/SearchSession.cpp](../../igi_linux_win/src/SearchSession.cpp) | Source module for SearchSession; defines `flashOverlay_`, `SearchSession::activate`, `SearchSession::dismiss`, `SearchSession::mlockCorpus`, `SearchSession::munlockAndWipeCorpus`, `SearchSession::onOcrFinished`, `SearchSession::onQueryChanged`. |
| [igi_linux_win/src/SearchSession.h](../../igi_linux_win/src/SearchSession.h) | Source module for SearchSession; defines `IScreenCapture`, `SearchOverlay`, `CaptureFlashOverlay`, `SearchSession`. |
| [igi_linux_win/src/core/ActiveWindowInspector.h](../../igi_linux_win/src/core/ActiveWindowInspector.h) | Source module for ActiveWindowInspector; defines `ActiveWindowInfo`. |
| [igi_linux_win/src/core/ActiveWindowInspector_linux.cpp](../../igi_linux_win/src/core/ActiveWindowInspector_linux.cpp) | Source module for ActiveWindowInspector linux; defines `names`, `queryActiveWindow`. |
| [igi_linux_win/src/core/ActiveWindowInspector_win.cpp](../../igi_linux_win/src/core/ActiveWindowInspector_win.cpp) | Source module for ActiveWindowInspector win; defines `queryActiveWindow`. |
| [igi_linux_win/src/core/Daemon.cpp](../../igi_linux_win/src/core/Daemon.cpp) | Source module for Daemon; defines `listener_`, `Daemon::version`, `Daemon::start`, `Daemon::stop`, `Daemon::checkAndWirePermissions`. |
| [igi_linux_win/src/core/Daemon.h](../../igi_linux_win/src/core/Daemon.h) | Source module for Daemon; defines `Daemon`. |
| [igi_linux_win/src/core/HotkeyListener.h](../../igi_linux_win/src/core/HotkeyListener.h) | Source module for HotkeyListener; defines `IHotkeyListener`. |
| [igi_linux_win/src/core/HotkeyListener_linux.cpp](../../igi_linux_win/src/core/HotkeyListener_linux.cpp) | Source module for HotkeyListener linux; defines `X11HotkeyListener`, `pollfd`, `runLoop`, `IHotkeyListener::create`. |
| [igi_linux_win/src/core/HotkeyListener_win.cpp](../../igi_linux_win/src/core/HotkeyListener_win.cpp) | Source module for HotkeyListener win; defines `WinHotkeyListener`, `runLoop`, `IHotkeyListener::create`. |
| [igi_linux_win/src/core/Permissions.h](../../igi_linux_win/src/core/Permissions.h) | Source module for Permissions; defines `PermissionStatus`. |
| [igi_linux_win/src/core/Permissions_stub.cpp](../../igi_linux_win/src/core/Permissions_stub.cpp) | Source module for Permissions stub; defines `preflightPermissions`, `openScreenRecordingSettings`, `openAccessibilitySettings`. |
| [igi_linux_win/src/core/ScreenCapture.h](../../igi_linux_win/src/core/ScreenCapture.h) | Source module for ScreenCapture; defines `CaptureResult`, `IScreenCapture`. |
| [igi_linux_win/src/core/ScreenCapture_qt.cpp](../../igi_linux_win/src/core/ScreenCapture_qt.cpp) | Source module for ScreenCapture qt; defines `QtScreenCapture`, `IScreenCapture::create`. |
| [igi_linux_win/src/core/ScreenGeometry.cpp](../../igi_linux_win/src/core/ScreenGeometry.cpp) | Source module for ScreenGeometry; defines `ScreenGeometry::infoFor`, `ScreenGeometry::screenAt`, `ScreenGeometry::screenByIndex`, `ScreenGeometry::toScreenPixels`. |
| [igi_linux_win/src/core/ScreenGeometry.h](../../igi_linux_win/src/core/ScreenGeometry.h) | Source module for ScreenGeometry; defines `QScreen`, `ScreenInfo`, `ScreenGeometry`. |
| [igi_linux_win/src/core/SecureWipe.h](../../igi_linux_win/src/core/SecureWipe.h) | Source module for SecureWipe. |
| [igi_linux_win/src/core/SecurityGuard.h](../../igi_linux_win/src/core/SecurityGuard.h) | Source module for SecurityGuard; defines `SecurityGuard`. |
| [igi_linux_win/src/core/SecurityGuard_linux.cpp](../../igi_linux_win/src/core/SecurityGuard_linux.cpp) | Source module for SecurityGuard linux; defines `SecurityGuard::checkDyldInjection`, `std::getenv`, `SecurityGuard::checkHardenedRuntime`, `SecurityGuard::checkDependencySignature`, `SecurityGuard::adviseDmaThreats`, `SecurityGuard::auditAccessibilityGrants`, `SecurityGuard::checkFileVault` and 1 more symbols. |
| [igi_linux_win/src/core/SecurityGuard_win.cpp](../../igi_linux_win/src/core/SecurityGuard_win.cpp) | Source module for SecurityGuard win; defines `SecurityGuard::checkDyldInjection`, `SecurityGuard::checkHardenedRuntime`, `SecurityGuard::checkDependencySignature`, `SecurityGuard::adviseDmaThreats`, `SecurityGuard::auditAccessibilityGrants`, `SecurityGuard::checkFileVault`, `SecurityGuard::runStartupChecks`. |
| [igi_linux_win/src/main.cpp](../../igi_linux_win/src/main.cpp) | Source module for main; defines `sigaction`, `posixSignalHandler`, `setupSignalHandler`, `main`, `QObject::connect`. |
| [igi_linux_win/src/ocr/ImageConverter.cpp](../../igi_linux_win/src/ocr/ImageConverter.cpp) | Source module for ImageConverter; defines `ImageConverter::qImageToPix`. |
| [igi_linux_win/src/ocr/ImageConverter.h](../../igi_linux_win/src/ocr/ImageConverter.h) | Source module for ImageConverter; defines `Pix`, `QImage`, `PixDeleter`, `ImageConverter`. |
| [igi_linux_win/src/ocr/OcrEngine.cpp](../../igi_linux_win/src/ocr/OcrEngine.cpp) | Source module for OcrEngine; defines `StripFuture`, `OcrEngine::resolveTessDataPath`, `OcrEngine::init`, `OcrEngine::recognizeAsync`. |
| [igi_linux_win/src/ocr/OcrEngine.h](../../igi_linux_win/src/ocr/OcrEngine.h) | Source module for OcrEngine; defines `TessBaseAPI`, `OcrEngine`, `TessWorker`, `Pix`. |
| [igi_linux_win/src/search/BoundingBox.h](../../igi_linux_win/src/search/BoundingBox.h) | Source module for BoundingBox; defines `WordBox`, `text`. |
| [igi_linux_win/src/search/FuzzySearch.cpp](../../igi_linux_win/src/search/FuzzySearch.cpp) | Source module for FuzzySearch; defines `FuzzySearch::normalize`, `FuzzySearch::calculateScore`, `FuzzySearch::topK`. |
| [igi_linux_win/src/search/FuzzySearch.h](../../igi_linux_win/src/search/FuzzySearch.h) | Source module for FuzzySearch; defines `Match`, `FuzzySearch`. |
| [igi_linux_win/src/ui/CaptureFlashOverlay.cpp](../../igi_linux_win/src/ui/CaptureFlashOverlay.cpp) | Source module for CaptureFlashOverlay; defines `QWidget`, `CaptureFlashOverlay::flash`, `CaptureFlashOverlay::paintEvent`. |
| [igi_linux_win/src/ui/CaptureFlashOverlay.h](../../igi_linux_win/src/ui/CaptureFlashOverlay.h) | Source module for CaptureFlashOverlay; defines `CaptureFlashOverlay`. |
| [igi_linux_win/src/ui/SearchOverlay.cpp](../../igi_linux_win/src/ui/SearchOverlay.cpp) | Source module for SearchOverlay; defines `HighlightOverlay::setHighlights`, `HighlightOverlay::paintEvent`, `SearchOverlay::activate`, `SearchOverlay::setHighlights`, `SearchOverlay::query`, `SearchOverlay::keyPressEvent`, `SearchOverlay::paintEvent`. |
| [igi_linux_win/src/ui/SearchOverlay.h](../../igi_linux_win/src/ui/SearchOverlay.h) | Source module for SearchOverlay; defines `HighlightOverlay`, `SearchOverlay`. |
| [igi_linux_win/tests/CMakeLists.txt](../../igi_linux_win/tests/CMakeLists.txt) | CMake build definition for igi_linux_win/tests; declares targets, dependency discovery, and compiler/build settings. |
| [igi_linux_win/tests/daemon_test.cpp](../../igi_linux_win/tests/daemon_test.cpp) | Test implementation for daemon test. |
| [igi_linux_win/tests/fuzzy_search_test.cpp](../../igi_linux_win/tests/fuzzy_search_test.cpp) | Test implementation for fuzzy search test. |
| [igi_linux_win/tests/hotkey_listener_test.cpp](../../igi_linux_win/tests/hotkey_listener_test.cpp) | Test implementation for hotkey listener test; defines `MockHotkeyListener`, `captureAndRegister`, `fireCallback`, `registeredState`. |
| [igi_linux_win/tests/image_converter_test.cpp](../../igi_linux_win/tests/image_converter_test.cpp) | Test implementation for image converter test; defines `makeFixture`. |
| [igi_linux_win/tests/main.cpp](../../igi_linux_win/tests/main.cpp) | Test implementation for main; defines `main`. |
| [igi_linux_win/tests/ocr_engine_test.cpp](../../igi_linux_win/tests/ocr_engine_test.cpp) | Test implementation for ocr engine test. |
| [igi_linux_win/tests/screen_geometry_test.cpp](../../igi_linux_win/tests/screen_geometry_test.cpp) | Test implementation for screen geometry test. |
| [igi_linux_win/tests/smoke_test.cpp](../../igi_linux_win/tests/smoke_test.cpp) | Test implementation for smoke test. |
| [resources/AppIcon.icns](../../resources/AppIcon.icns) | Binary resource for AppIcon; 1,979,591 bytes. |
| [resources/AppIcon.png](../../resources/AppIcon.png) | Image or interface asset for AppIcon. Source/asset rights remain separate from software-license terms. |
| [resources/Igi.entitlements](../../resources/Igi.entitlements) | Tracked Igi resource in resources; 794 bytes. |
| [resources/Info.plist](../../resources/Info.plist) | Tracked Info resource in resources; 2,290 bytes. |
| [resources/igi.desktop](../../resources/igi.desktop) | Linux desktop launcher for igi; records the command, icon, and desktop integration settings. |
| [resources/igi.sb](../../resources/igi.sb) | Tracked igi resource in resources; 1,822 bytes. |
| [resources/resources.qrc](../../resources/resources.qrc) | Tracked resources resource in resources; 135 bytes. |
| [scripts/sandbox_test.sh](../../scripts/sandbox_test.sh) | Command-line automation for sandbox test; review prerequisites and side effects before executing. |
| [scripts/sign_and_notarize.sh](../../scripts/sign_and_notarize.sh) | Command-line automation for sign and notarize; review prerequisites and side effects before executing. |
| [scripts/verify_deps.py](../../scripts/verify_deps.py) | scripts/verify_deps.sh — Dependency integrity check (Python 3, no bash 4 required). Defines `brew_prefix`, `brew_version`, `sha256_file`, `find_dylib`, `collect`, `load_lock` and 2 more top-level symbols. |
| [scripts/verify_deps.sh](../../scripts/verify_deps.sh) | Command-line automation for verify deps; review prerequisites and side effects before executing. |
| [src/SearchSession.cpp](../../src/SearchSession.cpp) | Source module for SearchSession; defines `flashOverlay_`, `SearchSession::activate`, `SearchSession::dismiss`, `SearchSession::mlockCorpus`, `SearchSession::munlockAndWipeCorpus`, `SearchSession::onOcrFinished`, `SearchSession::onQueryChanged`. |
| [src/SearchSession.h](../../src/SearchSession.h) | Source module for SearchSession; defines `IScreenCapture`, `SearchOverlay`, `CaptureFlashOverlay`, `SearchSession`. |
| [src/core/ActiveWindowInspector.h](../../src/core/ActiveWindowInspector.h) | Source module for ActiveWindowInspector; defines `ActiveWindowInfo`. |
| [src/core/ActiveWindowInspector.mm](../../src/core/ActiveWindowInspector.mm) | Tracked ActiveWindowInspector resource in src/core; 4,022 bytes. |
| [src/core/ActiveWindowInspector_linux.cpp](../../src/core/ActiveWindowInspector_linux.cpp) | Source module for ActiveWindowInspector linux; defines `names`, `queryActiveWindow`. |
| [src/core/ActiveWindowInspector_win.cpp](../../src/core/ActiveWindowInspector_win.cpp) | Source module for ActiveWindowInspector win; defines `queryActiveWindow`. |
| [src/core/AppPolicy.mm](../../src/core/AppPolicy.mm) | Tracked AppPolicy resource in src/core; 376 bytes. |
| [src/core/Daemon.cpp](../../src/core/Daemon.cpp) | Source module for Daemon; defines `listener_`, `Daemon::version`, `Daemon::start`, `Daemon::stop`, `Daemon::checkAndWirePermissions`. |
| [src/core/Daemon.h](../../src/core/Daemon.h) | Source module for Daemon; defines `Daemon`. |
| [src/core/HotkeyListener.h](../../src/core/HotkeyListener.h) | Source module for HotkeyListener; defines `IHotkeyListener`. |
| [src/core/HotkeyListener.mm](../../src/core/HotkeyListener.mm) | Tracked HotkeyListener resource in src/core; 4,746 bytes. |
| [src/core/HotkeyListener_linux.cpp](../../src/core/HotkeyListener_linux.cpp) | Source module for HotkeyListener linux; defines `X11HotkeyListener`, `pollfd`, `runLoop`, `IHotkeyListener::create`. |
| [src/core/HotkeyListener_win.cpp](../../src/core/HotkeyListener_win.cpp) | Source module for HotkeyListener win; defines `WinHotkeyListener`, `runLoop`, `IHotkeyListener::create`. |
| [src/core/Permissions.h](../../src/core/Permissions.h) | Source module for Permissions; defines `PermissionStatus`. |
| [src/core/Permissions.mm](../../src/core/Permissions.mm) | Tracked Permissions resource in src/core; 1,597 bytes. |
| [src/core/Permissions_stub.cpp](../../src/core/Permissions_stub.cpp) | Source module for Permissions stub; defines `preflightPermissions`, `openScreenRecordingSettings`, `openAccessibilitySettings`. |
| [src/core/PlatformHotkey.h](../../src/core/PlatformHotkey.h) | Source module for PlatformHotkey. |
| [src/core/ScreenCapture.h](../../src/core/ScreenCapture.h) | Source module for ScreenCapture; defines `CaptureResult`, `IScreenCapture`. |
| [src/core/ScreenCapture.mm](../../src/core/ScreenCapture.mm) | Tracked ScreenCapture resource in src/core; 4,725 bytes. |
| [src/core/ScreenCapture_qt.cpp](../../src/core/ScreenCapture_qt.cpp) | Source module for ScreenCapture qt; defines `QtScreenCapture`, `findBestScreen`, `IScreenCapture::create`. |
| [src/core/ScreenGeometry.cpp](../../src/core/ScreenGeometry.cpp) | Source module for ScreenGeometry; defines `ScreenGeometry::infoFor`, `ScreenGeometry::screenAt`, `ScreenGeometry::screenByIndex`, `ScreenGeometry::toScreenPixels`. |
| [src/core/ScreenGeometry.h](../../src/core/ScreenGeometry.h) | Source module for ScreenGeometry; defines `QScreen`, `ScreenInfo`, `ScreenGeometry`. |
| [src/core/SecureWipe.h](../../src/core/SecureWipe.h) | Source module for SecureWipe. |
| [src/core/SecurityGuard.h](../../src/core/SecurityGuard.h) | Source module for SecurityGuard; defines `SecurityGuard`. |
| [src/core/SecurityGuard.mm](../../src/core/SecurityGuard.mm) | Tracked SecurityGuard resource in src/core; 20,337 bytes. |
| [src/core/SecurityGuard_linux.cpp](../../src/core/SecurityGuard_linux.cpp) | Source module for SecurityGuard linux; defines `SecurityGuard::checkDyldInjection`, `std::getenv`, `SecurityGuard::checkHardenedRuntime`, `SecurityGuard::checkDependencySignature`, `SecurityGuard::adviseDmaThreats`, `SecurityGuard::auditAccessibilityGrants`, `SecurityGuard::checkFileVault` and 1 more symbols. |
| [src/core/SecurityGuard_win.cpp](../../src/core/SecurityGuard_win.cpp) | Source module for SecurityGuard win; defines `SecurityGuard::checkDyldInjection`, `SecurityGuard::checkHardenedRuntime`, `SecurityGuard::checkDependencySignature`, `SecurityGuard::adviseDmaThreats`, `SecurityGuard::auditAccessibilityGrants`, `SecurityGuard::checkFileVault`, `SecurityGuard::runStartupChecks`. |
| [src/main.cpp](../../src/main.cpp) | Source module for main; defines `sigaction`, `posixSignalHandler`, `setupSignalHandler`, `main`, `QObject::connect`. |
| [src/ocr/ImageConverter.cpp](../../src/ocr/ImageConverter.cpp) | Source module for ImageConverter; defines `ImageConverter::qImageToPix`. |
| [src/ocr/ImageConverter.h](../../src/ocr/ImageConverter.h) | Source module for ImageConverter; defines `Pix`, `QImage`, `PixDeleter`, `ImageConverter`. |
| [src/ocr/OcrEngine.cpp](../../src/ocr/OcrEngine.cpp) | Source module for OcrEngine; defines `StripFuture`, `OcrEngine::resolveTessDataPath`, `OcrEngine::init`, `OcrEngine::recognizeAsync`. |
| [src/ocr/OcrEngine.h](../../src/ocr/OcrEngine.h) | Source module for OcrEngine; defines `TessBaseAPI`, `OcrEngine`, `TessWorker`, `Pix`. |
| [src/ocr/PdfIngestor.cpp](../../src/ocr/PdfIngestor.cpp) | Source module for PdfIngestor; defines `PdfIngestor::isAvailable`, `PdfIngestor::ingestPdf`. |
| [src/ocr/PdfIngestor.h](../../src/ocr/PdfIngestor.h) | Source module for PdfIngestor; defines `OcrEngine`, `PdfIngestor`. |
| [src/search/BoundingBox.h](../../src/search/BoundingBox.h) | Source module for BoundingBox; defines `WordBox`, `text`. |
| [src/search/FuzzySearch.cpp](../../src/search/FuzzySearch.cpp) | Source module for FuzzySearch; defines `FuzzySearch::normalize`, `FuzzySearch::calculateScore`, `FuzzySearch::topK`. |
| [src/search/FuzzySearch.h](../../src/search/FuzzySearch.h) | Source module for FuzzySearch; defines `Match`, `FuzzySearch`. |
| [src/ui/CaptureFlashOverlay.cpp](../../src/ui/CaptureFlashOverlay.cpp) | Source module for CaptureFlashOverlay; defines `QWidget`, `CaptureFlashOverlay::setFlashOpacity`, `CaptureFlashOverlay::flash`, `CaptureFlashOverlay::paintEvent`. |
| [src/ui/CaptureFlashOverlay.h](../../src/ui/CaptureFlashOverlay.h) | Source module for CaptureFlashOverlay; defines `CaptureFlashOverlay`, `flashOpacity`. |
| [src/ui/SearchOverlay.cpp](../../src/ui/SearchOverlay.cpp) | Source module for SearchOverlay; defines `HighlightOverlay::setHighlights`, `HighlightOverlay::paintEvent`, `SearchOverlay::activate`, `SearchOverlay::paintEvent`, `SearchOverlay::setMatchInfo`, `SearchOverlay::setHighlights`, `SearchOverlay::query` and 4 more symbols. |
| [src/ui/SearchOverlay.h](../../src/ui/SearchOverlay.h) | Source module for SearchOverlay; defines `QLabel`, `HighlightOverlay`, `SearchOverlay`. |
| [test_cr.cpp](../../test_cr.cpp) | Source module for test cr; defines `main`. |
| [test_cr2.cpp](../../test_cr2.cpp) | Source module for test cr2; defines `main`. |
| [test_dump_corpus.patch](../../test_dump_corpus.patch) | Source patch for test dump corpus; apply only to the documented upstream revision. |
| [test_fastpath.cpp](../../test_fastpath.cpp) | Source module for test fastpath; defines `main`. |
| [test_fuzzy.cpp](../../test_fuzzy.cpp) | Source module for test fuzzy; defines `main`. |
| [test_search.cpp](../../test_search.cpp) | Source module for test search; defines `main`. |
| [test_split.cpp](../../test_split.cpp) | Source module for test split; defines `main`. |
| [test_tess.py](../../test_tess.py) | Automated checks for tess. Connects `PIL`, `pytesseract`, `sys`. |
| [tests/CMakeLists.txt](../../tests/CMakeLists.txt) | CMake build definition for tests; declares targets, dependency discovery, and compiler/build settings. |
| [tests/daemon_test.cpp](../../tests/daemon_test.cpp) | Test implementation for daemon test. |
| [tests/fuzzy_search_test.cpp](../../tests/fuzzy_search_test.cpp) | Test implementation for fuzzy search test. |
| [tests/hotkey_listener_test.cpp](../../tests/hotkey_listener_test.cpp) | Test implementation for hotkey listener test; defines `MockHotkeyListener`, `captureAndRegister`, `fireCallback`, `registeredState`. |
| [tests/image_converter_test.cpp](../../tests/image_converter_test.cpp) | Test implementation for image converter test; defines `makeFixture`. |
| [tests/main.cpp](../../tests/main.cpp) | Test implementation for main; defines `main`. |
| [tests/ocr_engine_test.cpp](../../tests/ocr_engine_test.cpp) | Test implementation for ocr engine test. |
| [tests/pdf_ingestor_test.cpp](../../tests/pdf_ingestor_test.cpp) | Test implementation for pdf ingestor test. |
| [tests/screen_capture_test.cpp](../../tests/screen_capture_test.cpp) | Test implementation for screen capture test. |
| [tests/screen_geometry_test.cpp](../../tests/screen_geometry_test.cpp) | Test implementation for screen geometry test. |
| [tests/search_session_test.cpp](../../tests/search_session_test.cpp) | Test implementation for search session test; defines `MockFailingScreenCapture`, `MockNullImageScreenCapture`. |
| [tests/smoke_test.cpp](../../tests/smoke_test.cpp) | Test implementation for smoke test. |
| [update_fuzzy_search.patch](../../update_fuzzy_search.patch) | Source patch for update fuzzy search; apply only to the documented upstream revision. |
