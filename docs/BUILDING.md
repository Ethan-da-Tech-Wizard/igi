# Building Igi OCR from Source

Igi is built with C++17, Qt 6, Tesseract OCR, and Leptonica.

## 1. 🏔 Arch Linux / Manjaro

### Prerequisites
Install the required dependencies via `pacman`:

```bash
sudo pacman -S --needed \
    cmake \
    gcc \
    make \
    pkgconf \
    qt6-base \
    tesseract \
    tesseract-data-eng \
    leptonica \
    libx11
```

### Build & Run Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Run Igi
```bash
./build/igi
```

---

## 2. 🐧 Ubuntu / Debian

### Prerequisites
```bash
sudo apt update && sudo apt install -y \
    cmake \
    g++ \
    make \
    pkg-config \
    qt6-base-dev \
    libtesseract-dev \
    tesseract-ocr-eng \
    libleptonica-dev \
    libx11-dev
```

### Build & Run Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## 3. 🍎 macOS

### Prerequisites via Homebrew
```bash
brew install cmake qt tesseract leptonica pkg-config
```

### Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## 4. 🪟 Windows (MSVC 2022 + vcpkg)

### Prerequisites
```cmd
vcpkg install qtbase tesseract leptonica --triplet x64-windows
cmake --preset windows
cmake --build build --config Release
```
