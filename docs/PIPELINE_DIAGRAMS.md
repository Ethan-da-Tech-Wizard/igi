# 🖼 Igi Visual Pipelines

As requested, here are the "imageified" concept blueprints of the Igi tool. Because visual image generators are conceptual, I have also paired each image with its strict, rigid engineering `mermaid` flowchart so you have both the aesthetic vision and the exact mathematical logic.

## 1. System UI & UX Concept
A visual representation of the minimalist, dark-mode floating search bar operating over a scanned medical document. 



---

## 2. High-Level Offline Architecture Pipeline
A conceptual visualization of the high-performance OCR data pipeline.



### Strict Logic Flow (Engineering Standard)
This is the programmed logic representing how the C++ application stays in memory and parallelises page-level OCR for multi-page PDFs.

> **Note on parallelism:** SIMD (single-instruction multiple-data, e.g.,
> AVX/NEON intrinsics) is *not* the same as thread-level parallelism. SIMD
> operates on multiple data elements within a single CPU instruction;
> threads run independent instruction streams. Igi parallelises **per
> page** via a bounded `QThreadPool` of OCR workers. Tesseract itself uses
> SIMD internally inside `Recognize()`, but Igi does not slice pixels
> across cores. See `docs/CHUNKSTONES` Chunk 6 for the concrete worker-pool
> design.

```mermaid
flowchart TD
    A[Global Hotkey Trigger] --> B{Detect Active Window}
    B -->|Browser/App| C1(Qt Screen Region Capture)
    B -->|PDF Viewer| C2(Detect Open File Path via AX API)

    C1 -->|QPixmap| F[QImage to PIX Converter]
    C2 -->|fz_open_document| C3(MuPDF Page Renderer)
    C3 -->|fz_pixmap per page| F

    F -->|PIX| Q[Bounded OCR Worker Pool<br/>N = hardware_concurrency - 1]
    Q -->|task: 1 page| W1[Worker: Tesseract Recognize page i]
    Q -->|task: 1 page| W2[Worker: Tesseract Recognize page j]
    Q -->|task: 1 page| W3[Worker: Tesseract Recognize page k]

    W1 & W2 & W3 --> R[(Volatile WordBox Vector<br/>text + x,y,w,h + page index)]

    R --> G{User types in search bar}
    G --> H[Fuzzy match returns top-K matches]
    H --> J[Render highlight polygon on the matched word's screen coordinates]
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
