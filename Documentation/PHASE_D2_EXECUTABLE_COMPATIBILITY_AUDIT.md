# CRICKET LEGACY — Phase D Step D.2: Executable Compatibility Audit
**Document ID:** `CL-D2-AUDIT-v1.0`  
**Classification:** Reverse Engineering, PE32 Binary Specification & DRM Compliance  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** BLOCKED — NO CANDIDATE EXECUTABLE PRESENT (Analysis: NOT TESTED; Requirements: FULLY SPECIFIED)  

---

## 1. Executive Summary

Step D.2 audits the technical compatibility and legal processing requirements of the target Windows executable for EA SPORTS Cricket 07.

Because **Step D.1 established that `Cricket07.exe` is currently absent from the workspace**, live PE header parsing, section entropy measurement, and hash validation of a local file are officially marked **NOT TESTED**.

However, the **upstream translation pipeline's strict binary input contract**, **SafeDisc DRM architecture**, and **profile patch expectations** were audited by inspecting [`translator/x86toc.py`](file:///d:/cricket07-browser-src/translator/x86toc.py), [`profiles/cricket07.h`](file:///d:/cricket07-browser-src/profiles/cricket07.h), [`profiles/cricket07.patches.json`](file:///d:/cricket07-browser-src/profiles/cricket07.patches.json), and [`tools/pe_survey.py`](file:///d:/cricket07-browser-src/tools/pe_survey.py).

---

## 2. Upstream Translation Contract & PE32 Requirements

The static recompiler ([`x86toc.py`](file:///d:/cricket07-browser-src/translator/x86toc.py)) uses `pefile` and `capstone` to disassemble and decompile x86 machine instructions directly from the binary. It requires the executable to satisfy the following specifications:

| PE Attribute | Required Value | Technical Rationale |
| :--- | :--- | :--- |
| **Machine Architecture** | `0x014C` (IMAGE_FILE_MACHINE_I386) | 32-bit Intel x86 machine code. PE32+ (x86_64) is incompatible with x86toc. |
| **Format** | PE32 (Portable Executable 32-bit) | Standard Windows 32-bit PE format. |
| **Preferred ImageBase** | `0x00400000` | Hardcoded guest memory space mapped by `cricket_memory` and `GUEST_IMAGE_BASE`. |
| **Entry Point RVA** | `0x002E88E0` (`VA: 0x006E88E0`) | `WinMainCRTStartup` entry point where CRT initialization begins. |
| **Large Address Aware** | Disabled (Flag `0x0020` unset) | Engine assumes a 2GB/3GB address space. Matches 4GB arena design. |
| **Relocations** | Relocations stripped (`0x0001`) | Binary is fixed at `0x00400000`; relies on direct virtual addresses. |
| **Section Names** | `.text`, `.rdata`, `.data`, `.bss` | Must match PE section layout configured in Stage 1 static data. |

---

## 3. SafeDisc Copy Protection Analysis

### 3.1 The Retail DRM Challenge
The retail CD-ROM release of EA SPORTS Cricket 07 (released October 2006 for Windows) was mastered with **SafeDisc v4** (Macrovision) disc copy protection:
1. **Encrypted Code Section (`.text`):** On the retail disc, the executable's `.text` code section is encrypted on disk. The true game machine code is not stored as standard x86 opcodes.
2. **Ring-0 Driver Requirement:** SafeDisc relies on a kernel-mode driver (`secdrv.sys`) to perform physical CD-ROM sector checks (weak-sector signatures) and decrypt the code pages directly into memory at runtime.
3. **Incompatibility with Static Decompilation:** Tools like Ghidra and `x86toc.py` parse raw bytes from the disk image. When presented with a SafeDisc-encrypted executable, Ghidra fails to identify function boundaries, produces nonsensical disassembly, and cannot generate `functions.json`.

### 3.2 Upstream Profile Warning & Strict Rules
As documented explicitly in upstream [`README.md:24-26`](file:///d:/cricket07-browser-src/README.md#L24-L26) and [`profiles/cricket07.h:3-4`](file:///d:/cricket07-browser-src/profiles/cricket07.h#L3-L4):
> *"The retail `Cricket07.exe` is wrapped by SafeDisc, whose encrypted code can't be translated. The build checks the executable against `profiles/cricket07.patches.json` and stops if it doesn't match."*

### 3.3 Strict Adherence to Non-Negotiable Rules 7 and 8
- **Rule 8:** *"Do not attempt to defeat, remove, bypass, patch around, or circumvent SafeDisc, DRM, copy protection, or licensing restrictions."*
- **Policy Compliance:** The Antigravity agent will **NEVER** strip DRM, generate a cracked executable, defeat copy protection, or bypass licensing checks.
- **Required Resolution:** Authentic integration strictly requires the user to supply a **legitimate, unencumbered executable** that has already been lawfully decrypted or released without disc-wrapping DRM (e.g. an official unencrypted patch or developer build).

---

## 4. Profile Byte Patches & Signature Masking

The upstream recompiler validates the executable using [`profiles/cricket07.patches.json`](file:///d:/cricket07-browser-src/profiles/cricket07.patches.json). Before translation proceeds, it asserts that the candidate executable matches expected byte sequences:

1. **Display Options Patch (`0x006E5613`):**
   - **Target Address:** `0x006E5613`, `0x006E5619`, `0x006E561F`
   - **Original Bytes:** `02`, `04`, `08`
   - **Patched Bytes:** `01`, `02`, `03`
   - **Purpose:** Fixes an original EA bug in `FUN_006E53D0` where the anti-aliasing menu index passed `0/1/2/3`, but the game compared with `2/4/8`, causing 2x AA to do nothing.

2. **Stream Buffer Data Race Patch (`0x0076A520`):**
   - **Target Address:** `0x0076A520`
   - **Length:** 176 bytes
   - **Original SHA-256:** `925c1285c4f92ee1039da58a498230bb38398001b923acc343d975a59183a984`
   - **Purpose:** Resolves a critical race condition in EA's stream buffer "take next block" routine (`0x0076A520`) that causes intro movies to freeze on a black screen.

3. **Entry Hook for Lazy Archive Views (`0x00652A00`):**
   - **Target Address:** `0x00652A00` (`hook_copy_source`)
   - **Target Function:** EA's custom memcpy implementation (`FUN_00652A00`).
   - **Purpose:** Intercepts every memory read from `data.gob` to trigger 64KB demand-paging.

---

## 5. Audit Verdict & Gate Status

| Verification Area | Requirement | Measured State | Status |
| :--- | :--- | :---: | :---: |
| **Executable Existence** | Candidate file present in workspace | Missing | **NOT TESTED** |
| **PE Header Analysis** | Machine 0x014C, Base 0x00400000 | Unverified (file absent) | **NOT TESTED** |
| **DRM Assessment** | Unprotected / Non-SafeDisc binary | Unverified (file absent) | **NOT TESTED** |
| **Profile Signature Match** | SHA-256 match at `0x0076A520` | Unverified (file absent) | **NOT TESTED** |
| **Pipeline Requirements** | Clear contract for x86toc.py | Fully specified in tooling | **AUDITED** |

### Status: BLOCKED
A legally usable, unencumbered `Cricket07.exe` is required before Step D.2 can transition to PASS.
