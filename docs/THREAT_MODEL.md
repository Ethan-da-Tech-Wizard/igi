# Igi: Threat Model & Attack Surface Analysis

**Classification:** Internal Security Documentation  
**Audience:** Security reviewers, HIPAA compliance officers, government evaluators  
**Last Updated:** May 2026

This document presents a complete, honest threat model for the Igi OCR search
utility. For each threat, we describe the attack in depth across three vectors:
**cyber** (remote/software), **physical** (in-person), and **other** (supply
chain, legal, environmental). We then state the current mitigation and any
residual risk.

---

## Threat 1 — Process Injection into Igi

### What the threat is
Because Igi holds the macOS Screen Recording permission, any code executing
*inside* Igi's process inherits that capability. The operating system does not
distinguish between Igi's legitimate code and injected foreign code — if it is
running in Igi's process, it can call `CGWindowListCreateImage` and capture
anything on screen.

### Cyber vector
An attacker who achieves remote code execution on the target machine — through
a memory-corruption bug in Tesseract (a C library with a history of CVEs), a
malicious Qt plugin loaded from a compromised path, or a poisoned Homebrew
formula that ships a backdoored version of leptonica — can inject a shared
library into Igi's address space using macOS's `DYLD_INSERT_LIBRARIES`
environment variable or by exploiting a heap overflow in the OCR pipeline. Once
inside, they spawn a thread that calls `CGWindowListCreateImage` in a loop,
encoding each frame as JPEG and posting it to an attacker-controlled server via
`NSURLSession` — all from within a process the OS trusts to see the screen.

A subtler variant: the attacker does not inject code at all. They compromise the
build pipeline and insert a call to `dispatch_async` in `ImageConverter.cpp`
that copies the `QImage` pixel buffer to a hidden file in `/var/folders`
(a path our sandbox allowlists as a macOS system requirement) before it is
passed to Tesseract. Because the copy happens before the `explicit_bzero`
teardown, the file survives and contains the raw screen pixels.

### Physical vector
An attacker with brief physical access to an unlocked Mac can open Terminal,
set `DYLD_INSERT_LIBRARIES=/path/to/evil.dylib`, and launch Igi. macOS's
Hardened Runtime (`--options runtime` in our codesign command) blocks
`DYLD_INSERT_LIBRARIES` on signed binaries — but only if Igi is signed. In
development, where we run unsigned debug builds, this protection does not exist.

### Other vector
A disgruntled employee on the build team modifies `OcrEngine.cpp` to include a
dead-looking `#ifdef DEBUG` block that, when a specific environment variable is
set, writes the corpus to disk before zeroing it. The change passes code review
because it appears to be a debugging aid. This is a supply chain / insider
threat.

### Current mitigations
- `--options runtime` in codesign command blocks `DYLD_INSERT_LIBRARIES` on
  release builds.
- No network entitlement in `Igi.entitlements` — a library injection that
  tries to open a socket is blocked by the OS sandbox.
- CI builds are reproducible; dependency versions are pinned.

### Residual risk
**Medium.** A sufficiently motivated attacker targeting a development machine
(where Hardened Runtime is not enforced) or a supply chain compromise of
Tesseract or leptonica remains a realistic concern. The mitigation is to run
only release-signed builds in production environments and to verify dependency
checksums in CI.

---

## Threat 2 — Memory Forensics on the Heap

### What the threat is
Between the time OCR completes and the user presses Escape, the `corpus_`
vector holds every word extracted from the screen — including patient names,
diagnoses, medication names, and record numbers — as plaintext `QString`
objects in heap memory. Even after `dismiss()` is called, those bytes sit in
freed memory until the allocator reuses that address range. On a lightly loaded
system, freed memory can persist readable for minutes.

### Cyber vector
A process running as the same user as Igi (any app the user has installed and
launched) can call `task_for_pid()` on Igi's PID, obtain a Mach task port, and
use `mach_vm_read()` to take a snapshot of Igi's entire heap at any point in
time. On macOS, `task_for_pid` on another process *requires* the caller to hold
the `com.apple.security.get-task-allow` entitlement or to run as root — but a
process the user willingly launched with Accessibility or Screen Recording
permission (e.g., a malicious "productivity app") can legitimately hold these.
The attacker does not need to inject into Igi at all; they just need to snapshot
its memory at the right moment.

A coarser version: after Igi exits, the attacker uses a user-space memory
recovery tool or `/dev/mem` (requires root on modern macOS) to scan the physical
memory pages that were returned to the OS. On machines without FileVault, those
pages may persist on disk if they were swapped.

### Physical vector
An attacker with a Thunderbolt-equipped external device and physical access
performs a **DMA (Direct Memory Access) attack** using a tool like `pcileech`.
Thunderbolt DMA attacks can read all system RAM from an external device in
seconds, bypassing all OS-level protections. This is a known attack against law
enforcement laptops. A medical records reviewer who leaves their MacBook
unlocked for 60 seconds while Igi has an active session is fully exposed.

A simpler physical variant: the attacker observes the screen directly — the
word typed in the search bar and the amber highlights reveal exactly what the
user is looking for and where it appears in the document. No technical exploit
required.

### Other vector
**Cold boot attack.** If the machine loses power immediately after an active Igi
session, DRAM retains its state for seconds to minutes at room temperature
(longer if chilled with compressed air). An attacker who can reboot the machine
from external media within that window can dump the contents of RAM before the
OS clears it. FileVault does not protect against this for data that was
decrypted and in active memory.

### Current mitigations
- `explicit_bzero()` now runs over every `QString` buffer in the corpus before
  `corpus_.clear()` — PHI strings are zeroed, not just freed (just implemented).
- `PixPtr` RAII deleter runs `explicit_bzero` on the Leptonica pixel buffer
  before `pixDestroy`.
- SEC-01 in the Risk Register calls for `mlock()` to prevent swap.

### Residual risk
**High without `mlock()`.** The `explicit_bzero` in `dismiss()` only fires when
the user deliberately presses Escape. If Igi crashes, is force-quit, or the
machine loses power during a session, the wipe never runs. `mlock()` (preventing
swap to disk) is the highest-priority remaining engineering gap.

---

## Threat 3 — Permission Inheritance by a Compromised Parent Process

### What the threat is
macOS permissions are granted to a process *and* to any process it spawns as a
child. If the Terminal or shell that launched Igi already held Screen Recording
permission, then any process spawned by that shell inherits the capability. An
attacker who can execute code in that same shell can call
`CGWindowListCreateImage` without ever touching Igi at all.

### Cyber vector
The attacker publishes a malicious npm package, Python package, or Homebrew
formula that, when installed, drops a `.zshrc` hook or a LaunchAgent plist.
The hook runs silently in the background, periodically calling the macOS screen
capture API. Because the Terminal process inherited Screen Recording permission
from a previous legitimate grant, this works without any further user action.
The attacker never needs to target Igi directly — they target the environment
Igi runs in.

A more targeted variant: the attacker compromises one of Igi's build
dependencies. When `cmake` or `make` runs during a build, the malicious build
step captures the screen and sends it out. The build system runs in the same
Terminal with the same inherited permissions.

### Physical vector
An attacker who obtains the user's macOS login password — through phishing, a
hardware keylogger attached to an external keyboard (invisible, the size of a
USB dongle), or simply watching them type — can log in as that user and their
session inherits all granted permissions immediately. They do not need to know
anything about Igi.

### Other vector
**MDM misconfiguration.** In an enterprise environment, a Mobile Device
Management policy that grants Screen Recording to a broad category of apps can
inadvertently grant it to a malicious app that matches the policy criteria. This
is an organizational process failure, not a code defect, but it directly affects
Igi's security posture.

### Current mitigations
- Production Igi runs as a signed bundle with Hardened Runtime.
- This threat is largely outside Igi's control — it is an operating system and
  environment hygiene problem.

### Residual risk
**Medium.** The correct mitigations are environmental: run Igi only from a
dedicated clinical user account; enforce MDM policies that grant Screen
Recording only to `com.igi.ocrsearch` by bundle identifier.

---

## Threat 4 — Hotkey Timing Covert Channel

### What the threat is
Igi's Accessibility permission means it watches global keyboard events. Any
other app that also holds Accessibility permission can do the same. An attacker
can observe the precise timestamp at which `Cmd+Shift+F` is pressed, building a
behavioral profile of when searches occur — inferring which patients are being
reviewed, how long reviews take, and when anomalies occur (e.g., a 3 AM search
pattern inconsistent with the clinician's schedule, possibly indicating a
credential-sharing violation).

This is a **covert channel**: no PHI is leaked directly, but metadata about PHI
access is leaked. Under HIPAA's minimum necessary rule and audit log
requirements, access timing is itself sensitive.

### Cyber vector
A malicious app registers its own global `NSEvent` monitor. Every time
`Cmd+Shift+F` fires, it logs a timestamp and the frontmost application name
(which reveals which EHR or document viewer was open at that moment). Over
weeks, this builds a detailed access log showing exactly when and how often
protected records are reviewed. The malicious app exfiltrates this log. The
attacker now has a complete audit trail of PHI access — which they can use for
blackmail, whistleblowing manipulation, or to identify patterns of illicit access
that the attacker themselves can exploit.

### Physical vector
An attacker observing the user's keyboard — directly or through camera footage
of the workspace (which exists in many clinical environments for patient safety
reasons) — can observe the hotkey sequence manually. Combined with knowledge
that Igi is installed, they know a search occurred at a specific time.

### Other vector
**Acoustic side channel.** Mechanical and even membrane keyboards produce
distinct sounds for different key combinations. A smartphone placed on a desk,
a smart speaker, or a compromised conference room system with a microphone can,
with sufficient signal processing, identify `Cmd+Shift+F` by its acoustic
signature. This is a published academic attack, not theoretical.

### Current mitigations
- Igi does not log access times itself — it generates no audit trail.
- Only the specific three-key combination is monitored via `NSEvent`; no other
  keystrokes are observed or stored.

### Residual risk
**Low for data breach; Medium for metadata leakage.** Cannot be eliminated
without removing the global hotkey. Organizational mitigation: ensure no
unnecessary apps hold Accessibility permission on clinical machines (enforced
via MDM).

---

## Threat 5 — Overcapture / Data Integrity Attack

### What the threat is
macOS Screen Recording permission is all-or-nothing. Once granted, Igi can
capture any window — not just the intended one. If the user presses
`Cmd+Shift+F` while a different sensitive window is accidentally in front, Igi
captures and OCRs that unintended window. More dangerously: an attacker with
Accessibility permission can manipulate which window is in front at the exact
moment of capture, causing Igi to display incorrect OCR output to the clinician
— a **data integrity attack** rather than a data theft attack.

### Cyber vector
**TOCTOU (Time-of-Check Time-of-Use) window manipulation.** An attacker with
Accessibility permission calls `AXUIElementPerformAction` with `kAXRaiseAction`
on a window they control at precisely the right moment — the milliseconds
between hotkey press and `CGWindowListCreateImage` execution. Their window
contains carefully crafted text: for example, a patient name that looks
legitimate but contains subtly incorrect medication dosages or allergy
information. The clinician sees Igi's amber highlights on what appears to be the
correct record, but the OCR corpus was built from the attacker's spoofed window.
This attack could directly cause a medical error.

### Physical vector
If two users share a workstation, one user can physically move the mouse to
bring a different window to the front just as the other user triggers the
hotkey. This requires physical presence but no technical knowledge.

### Other vector
**Remote desktop / screen sharing session.** In a telehealth or remote support
session, a participant who can influence the remote desktop session (through
legitimate screen control or through a compromised screen sharing app) can
manipulate window focus before the hotkey fires.

### Current mitigations
- `ActiveWindowInspector` captures the window by PID and intersection area,
  making the race window smaller.
- `capturedScreenRect_` positioning means an accidental capture of the wrong
  window will visually place highlights in the wrong screen location,
  alerting an observant user.

### Residual risk
**Low for data theft; Medium for integrity in shared-workstation environments.**
The TOCTOU window is architecturally unavoidable. The recommended mitigation is
a visible capture flash indicator (a brief border or menu-bar pulse) so users
can verify the correct window was captured before trusting the search results.

---

## Summary Risk Matrix

| ID | Threat | Cyber | Physical | Other | Overall | Mitigated? |
|:---|:---|:---:|:---:|:---:|:---:|:---:|
| T-1 | Process injection | High | Medium | Medium | **High** | Partial |
| T-2 | Heap memory forensics | Medium | High | High | **High** | Partial* |
| T-3 | Permission inheritance | High | Medium | Medium | **Medium** | Env only |
| T-4 | Hotkey timing channel | Low | Low | Medium | **Low** | No |
| T-5 | Overcapture / integrity | Medium | Low | Low | **Medium** | Partial |

\* `explicit_bzero` implemented. `mlock()` not yet implemented.

---

## Prioritized Remaining Engineering Actions

| Priority | Action | Threat |
|:---:|:---|:---:|
| 🔴 1 | Implement `mlock()` on corpus backing store — prevents swap to disk | T-2 |
| 🔴 2 | Add crash/signal handler that calls `dismiss()` before exit | T-2 |
| 🟡 3 | Visible capture flash indicator (menu bar pulse or border flash) | T-5 |
| 🟡 4 | Document MDM allowlist policy for `com.igi.ocrsearch` | T-3 |
| 🟢 5 | Pin and checksum all third-party dependency versions in CI | T-1 |
