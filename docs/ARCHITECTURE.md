# Igi: Core Systems Architecture

This document maps the exact C++ components, external dependencies, and memory lifecycles required to execute the ultra-low latency OCR pipeline within a zero-storage constraint.

## 1. High-Level Component Architecture
This diagram outlines the distinct modules of the application and how they interact. The entire system is built around the Qt Framework for UI/Events and Tesseract for data processing.

```mermaid
graph TD
    subgraph macOS Environment
        A[macOS Event Loop]
        B[Active Window / Screen Buffer]
    end

    subgraph Igi Daemon Engine (C++)
        C(Global Hotkey Listener)
        D(Qt Screen Capture Engine)
        E[Volatile RAM Data Array]
        
        subgraph OCR Pipeline
            F(Leptonica Image Processor)
            G(Tesseract C++ API)
            K(MuPDF Memory Loader)
        end
        
        subgraph UI Overlay System
            H(Qt Frameless Search Bar)
            I(Fuzzy Match Algorithm)
            J(Qt Transparent Highlight Polygon)
        end
    end

    A -- "Cmd+Shift+F" --> C
    C --> D
    B -- "Read Pixels" --> D
    B -- "Detect PDF File Path" --> K
    D -- "QPixmap -> PIX Array" --> F
    K -- "Raw Bytes -> PIX Array" --> F
    F --> G
    G -- "Words + Bounding Boxes" --> E
    
    C --> H
    H -- "User Input" --> I
    E -- "Data Lookup" --> I
    I -- "Coordinates (x,y,w,h)" --> J
    J -- "Render Highlight" --> B
```

---

## 2. The Microsecond Execution Sequence
This sequence diagram illustrates the exact chronological workflow of a search operation. It highlights the strict initialization and destruction phases required to support HIPAA-aligned workflows (see `docs/DECISIONS.md` D-007 for the read/write boundary).

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant OS as macOS
    participant Igi as Igi Daemon
    participant Qt as Qt Capture & UI
    participant OCR as Tesseract Engine
    participant RAM as Volatile Memory

    Note over Igi, OCR: Daemon Startup (Pre-loading)
    Igi->>OCR: Init("eng")
    OCR->>RAM: Load Language Model
    Igi->>OS: RegisterEventHotKey()

    Note over User, RAM: Active Search Lifecycle
    User->>OS: Press Cmd+Shift+F
    OS->>Igi: Hotkey Event Triggered
    
    alt Active Window is Website/App
        Igi->>Qt: Grab Active Window Buffer
        Qt->>RAM: Store QPixmap
        Igi->>OCR: Feed Pixel Array
    else Active Window is PDF
        Igi->>OS: Get Open PDF File Path
        Igi->>RAM: MuPDF Stream File to Memory
        Igi->>OCR: Feed Extracted Pixel Array
    end
    
    OCR->>RAM: Extract Words & Coordinates
    
    Igi->>Qt: Spawn Floating Search Bar
    User->>Qt: Types Search Query
    Qt->>RAM: Execute Fuzzy Match
    RAM-->>Qt: Return Top Match Coordinates
    Qt->>OS: Draw Transparent Yellow Overlay
    
    Note over User, RAM: Purge & Teardown Phase
    User->>Qt: Presses Escape (Close)
    Qt->>Igi: Send Close Event
    Igi->>RAM: Destroy QPixmap
    Igi->>RAM: Destroy Bounding Box Array
    Igi->>Qt: Destroy Search Bar & Overlay
    Note over Igi, RAM: System returns to Idle. Zero Data Stored.
```

---

## 3. Memory & Security Isolation Layer
Because Igi is a zero-storage application, it is critical to understand the boundaries of what the C++ application is allowed to access.

```mermaid
block-beta
    columns 3
    
    block:UserSpace:3
        A("Igi C++ Application")
    end

    space:3
    
    block:Allowed:1
        B("✅ Read Screen Buffer")
        C("✅ Allocate Heap Memory")
        D("✅ Read Keystrokes (UI)")
    end
    
    space:1
    
    block:Denied:1
        E("❌ Write to File System")
        F("❌ Connect to Network")
        G("❌ Elevate Root Privileges")
    end
```
