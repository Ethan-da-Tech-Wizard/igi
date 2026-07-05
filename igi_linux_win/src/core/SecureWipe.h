#pragma once

#include <cstddef>   // size_t
#include <cstring>

// ── igi_bzero: compiler-elision-proof memory wipe ────────────────────────────
//
// DECISIONS.md D-006: Use explicit_bzero (or an equivalent volatile loop) to
// wipe sensitive buffers. Standard memset() on a buffer that is about to be
// freed is classified as a "dead store" by modern optimisers and is legally
// elided under the C++ abstract machine model — meaning the PHI data is NOT
// actually zeroed despite the call appearing in source.
//
// explicit_bzero() is specified by POSIX.1-2017 and available on Apple
// platforms since macOS 10.12, but it is NOT declared in any SDK header —
// it is a symbol-level export only. We therefore declare it ourselves and
// provide a volatile-loop fallback for compilers/platforms that lack it.
//
// Usage: igi_bzero(ptr, byte_count) — equivalent to explicit_bzero.

#if defined(__APPLE__) || defined(__linux__)
// explicit_bzero is exported by libSystem/libc on Apple and Linux, but is NOT
// declared in any public SDK header and is not reliably linkable from all
// build configurations. We use the volatile-loop equivalent which the C++
// standard guarantees cannot be elided (volatile side-effects are observable),
// and which has zero linker dependencies.
inline void igi_bzero(void* buf, std::size_t len) noexcept {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(buf);
    const volatile unsigned char* end = p + len;
    while (p != end) {
        *p++ = 0;
    }
}
#else
// Portable fallback — same volatile loop.
inline void igi_bzero(void* buf, std::size_t len) noexcept {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(buf);
    while (len--) {
        *p++ = 0;
    }
}
#endif
