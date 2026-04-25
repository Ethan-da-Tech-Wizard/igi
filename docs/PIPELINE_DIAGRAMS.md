# 🖼 Igi Visual Pipelines

As requested, here are the "imageified" concept blueprints of the Igi tool. Because visual image generators are conceptual, I have also paired each image with its strict, rigid engineering `mermaid` flowchart so you have both the aesthetic vision and the exact mathematical logic.

## 1. System UI & UX Concept
A visual representation of the minimalist, dark-mode floating search bar operating over a scanned medical document. 



---

## 2. High-Level Offline Architecture Pipeline
A conceptual visualization of the high-performance OCR data pipeline.



### Strict Logic Flow (Engineering Standard)
This is the actual, programmed logic representing how the C++ application bypasses the hard drive and multi-threads the 112-page document.

```mermaid
flowchart TD
    A[Global Hotkey Trigger] --> B{Detect Active Window}
    B -->|Browser/App| C1(Qt Screen Region Capture)
    B -->|PDF Viewer| C2(Detect Open File Path)
    
    C1 -->|Visual Data Array| D{SIMD Multithreading Split}
    C2 -->|C++ mmap| C3(MuPDF Memory Loader)
    C3 --> D
    D -->|Thread 1| D1[Core 1: Pages/Frames 1-25]
    D -->|Thread 2| D2[Core 2: Pages/Frames 26-50]
    D -->|Thread 3| D3[Core 3: Pages/Frames 51-75]
    D -->|Thread 4| D4[Core 4: Pages/Frames 76-112]
    
    D1 & D2 & D3 & D4 --> E(Tesseract Engine)
    E --> F[(Volatile RAM Data Array)]
    
    F --> G{User Clicks 'Next' Navigation}
    G --> H[Qt UI Instantly Renders Page 55 Overlay from RAM]
```

---

## 3. The Algorithmic Matrix
A conceptual visualization of the neural networks and mathematical algorithms processing the text in the background.



### Strict Algorithmic Data Flow
This is the step-by-step mathematical lifecycle of a single word moving from raw pixels to a highlighted result on the screen.

```mermaid
flowchart LR
    A[Raw RGB Pixels] --> B[Otsu's Method Binarization]
    B -->|Maximizes Contrast| C(LSTM Neural Network OCR)
    C -->|Text & Coordinates| D[Wagner-Fischer Distance Matrix]
    D -->|O N Bitap Match| E{Fuzzy Match Confidence > 90%?}
    E -- Yes --> F[Affine Matrix Transform]
    F -->|Scale & Translate (x, y)| G[Draw Yellow Polygon]
    E -- No --> H[Discard Pointer]
```
