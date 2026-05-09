# Igi OCR Search Engine — MDM Deployment Guide

Igi is designed for zero-trust clinical environments. While the utility employs active startup checks (`SecurityGuard`) to log potential security degradation (like Thunderbolt DMA vulnerability or unapproved Accessibility processes), it **relies on OS-level enforcement** to permanently lock down its permissions environment.

This guide provides the necessary payloads to configure a macOS Mobile Device Management (MDM) profile (e.g., Jamf Pro, Mosyle, Kandji) that hardens the macOS host against T-3 (Permission Inheritance), T-4 (Timing Covert Channel), and T-5 (Overcapture) threats.

---

## 1. PPPC (Privacy Preferences Policy Control) Profile
Igi requires two sensitive macOS permissions. To prevent an attacker from socially engineering a clinician into granting these permissions to a malicious app, you must deploy a PPPC payload that:
1. Pre-approves Igi for both permissions.
2. (Optionally) Denies the permissions to any other non-essential clinical tool.

### Igi Bundle Identifier & Code Requirement
Your MDM will ask for the following identity details:
- **Identifier**: `com.igi.ocrsearch`
- **Identifier Type**: `Bundle ID`
- **Code Requirement**: 
  *(You can extract this exactly from the signed binary via `codesign -dr - /Applications/igi.app`)*

### Required PPPC Grants
Deploy the following payloads under the `com.apple.TCC.configuration-profile-policy` domain:

#### A. Screen Recording (T-5 Mitigation)
- **Service**: `ScreenCapture`
- **Authorization**: `AllowStandardUserToSetSystemService` (Since macOS 11, MDM cannot silently grant Screen Recording; it can only allow standard users to grant it without admin rights). 

#### B. Accessibility (T-4 / T-5 Mitigation)
- **Service**: `Accessibility`
- **Authorization**: `Allow` (MDM *can* silently grant Accessibility).

> **SECURITY ENFORCEMENT**: Audit your fleet's existing PPPC profiles. **Revoke** `Accessibility` and `ScreenCapture` grants for any bundle ID that does not strictly require them. Any app holding Accessibility can covertly monitor `Cmd+Shift+F` keystrokes and manipulate window focus to execute a TOCTOU injection attack against Igi.

---

## 2. Hardware Security & DMA Mitigation
Igi uses `mlock()` and `explicit_bzero()` to prevent protected health information (PHI) from being paged to disk or forensically recovered from the heap. However, these software controls are entirely bypassed by **Direct Memory Access (DMA)** attacks over Thunderbolt (T-2 Threat).

### Thunderbolt Security Level
Push a configuration profile to set the Thunderbolt Security level to `Secure` or `Full Security`. 

- **Domain**: `com.apple.MCX`
- **Key**: `force-thunderbolt-security` (Or configure natively in your MDM's hardware restriction pane).

*Note: Apple Silicon Macs use USB4 and inherently restrict DMA without kernel extensions, but Intel Macs remain highly vulnerable if Thunderbolt Security is set to "None".*

---

## 3. Storage Security (Cold Boot Mitigation)
If an attacker forcibly shuts down a clinician's workstation, they can extract the physical SSD. If the OS had paged data *before* Igi was launched, legacy swap files might still contain fragments of PHI.

- **Enforcement**: Ensure a **FileVault** enforcement profile (`com.apple.MCX.FileVault2`) is deployed and active.
- **Monitoring**: Igi runs an internal `fdesetup status` check on launch (`SEC-GUARD-06`). If FileVault is off, it logs a severe warning to the macOS Unified Log. Ensure your MDM syslog collector forwards `[igi]` subsystem warnings to your SIEM.

---

## 4. Syslog / EDR Monitoring
Igi logs security telemetry via `NSLog`, which is captured by the macOS Unified Log. Configure your EDR (e.g., CrowdStrike, SentinelOne) or MDM log collector to alert on the following string patterns:

| Log Pattern | Severity | Meaning |
|---|---|---|
| `[igi] SECURITY ABORT` | **CRITICAL** | Process injection (`DYLD_*`), tampered binary, or missing Hardened Runtime detected. Igi aborted before reading PHI. |
| `[igi] SECURITY WARNING: SEC-GUARD-06 FAIL` | **HIGH** | FileVault is off. Machine is highly vulnerable to physical extraction. |
| `[igi] SECURITY WARNING: SEC-GUARD-04` | **MEDIUM** | Thunderbolt Security is set to None. DMA attacks are possible. |
| `[igi] SECURITY WARNING: SEC-GUARD-05` | **INFO/AUDIT** | A non-standard application was observed running with potential Accessibility rights. Verify it is authorised. |

---
*End of Deployment Guide. Ensure these profiles are active before deploying Igi to clinical end-users.*
