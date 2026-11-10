# CRICKET LEGACY — Phase D Step D.1: Game File Availability Audit
**Document ID:** `CL-D1-AUDIT-v1.0`  
**Classification:** Inventory Audit, Legal Compliance & Workspace File Survey  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** COMPLETE — ALL AUTHENTIC GAME ASSETS & EXECUTABLES CURRENTLY ABSENT  

---

## 1. Executive Summary

As part of **Phase D: Authentic Game Integration Readiness**, Step D.1 performed an exhaustive, non-destructive file availability audit across the active project repository (`d:\cricket07 mobile app`), the upstream translation repository (`d:\cricket07-browser-src`), and designated scratch/temp directories (`d:\tmp`, `d:\te`, `d:\testing`).

The objective was to identify whether legitimate, lawful copies of EA SPORTS Cricket 07 files (executable, asset archives, audio streams, configuration files) exist locally on the machine.

### Audit Verdict: ABSENT (0 Authentic Game Files Found)
- **Authentic Executable (`Cricket07.exe`):** **NOT FOUND** (0 instances).
- **Primary Game Archive (`data.gob`):** **NOT FOUND** (0 instances).
- **Secondary Archives (`*.big`, `*.fsh`):** **NOT FOUND** (0 instances).
- **Audio Streams (`*.asf`, `*.dat`):** **NOT FOUND** (0 instances).
- **Game DLLs & Configuration:** **NOT FOUND** (0 instances).

---

## 2. Methodology & Discovery Scope

The discovery search was performed using targeted PowerShell filesystem inspection restricted to designated directories to prevent runaway disk I/O:
1. Target Directory 1: `d:\cricket07 mobile app` (Android host codebase)
2. Target Directory 2: `d:\cricket07-browser-src` (Upstream translation framework)
3. Target Directory 3: `d:\tmp`, `d:\te`, `d:\testing` (Local scratch folders)

File filters checked:
- Executables: `*.exe`, `*.dll`
- Game Archives: `*.gob`, `*.big`, `*.viv`, `*.tar`
- Graphics & Textures: `*.fsh`, `*.tga`, `*.bmp`
- Audio & Video: `*.asf`, `*.dat`, `*.mad`, `*.vp6`, `*.wav`

---

## 3. Detailed File Inventory by Location

### 3.1 Upstream Source Repository: `d:\cricket07-browser-src`
- **Total Files:** 67 files
- **Total Size:** 1,001,225 bytes (~1.00 MB)
- **Classification:** 100% open-source tooling, translation scripts, and emulation wrappers. Zero proprietary EA game files.
- **Inventory Summary:**
  - `translator/x86toc.py` (78,435 bytes): Static recompiler script (Python).
  - `ghidra/export.sh`, `ExportFunctions.java`: Ghidra headless export scripts.
  - `profiles/cricket07.h`, `cricket07.patches.json`: Compatibility definitions and patch metadata.
  - `host/*.c`: Win32/D3D8/DirectSound compatibility source code.
  - `runtime/*.c`, `runtime/*.h`: Register state and function dispatch runtime.
  - `web/*.js`, `web/*.html`: WebAssembly/Emscripten front-end wrapper.

### 3.2 Android Host Project: `d:\cricket07 mobile app`
- **Native Source Files (`android-native-prototype/src`):** 35 C source/header files implementing Stages 1 through 5 of Option B.
- **Compiled Binaries (`jniLibs/`):**
  - `libcricket_native_harness.so` (arm64-v8a: 1,236 KB; x86_64: 1,163 KB)
  - `libwgpu_native.so` (arm64-v8a: 14,888 KB; x86_64: 18,124 KB)
- **Packaged APK:** `android-app/app/build/outputs/apk/debug/cricket-legacy-debug.apk` (40.9 MB)
- **Third-Party Build Artifacts:** `third_party/wgpu/target` contains Rust `cargo` intermediate build helper executables (e.g. `build-script-build.exe`). None are game-related.

### 3.3 Scratch & Temporary Directories (`d:\tmp`, `d:\te`, `d:\testing`)
- Scanned recursively: Zero game-related binaries or archives found.

---

## 4. Expected Authentic Game File Structure

Based on analysis of [`profiles/cricket07.h`](file:///d:/cricket07-browser-src/profiles/cricket07.h), [`host/core.c`](file:///d:/cricket07-browser-src/host/core.c), and [`tools/ea_assets.py`](file:///d:/cricket07-browser-src/tools/ea_assets.py), the engine expects the following file layout upon integration:

```text
<GameInstallRoot>/
├── Cricket07.exe          # Unprotected 32-bit PE executable (~4.5 MB unpacked)
├── data.gob               # Primary RefPack-compressed game asset archive (~325 MB)
├── fe.big                 # Front-end UI, menus, fonts, textures
├── audio.big              # Commentary, crowd noise, ball impacts, UI sounds
├── motion.big             # Player motion capture animation streams (~64 MB)
├── music.big              # Soundtrack and menu music
├── teams.big              # Rosters, player attributes, tournaments
├── stadiums.big           # 3D stadium geometry, pitch textures, lighting
└── userdata/              # User profiles, saves ("My Documents\EA SPORTS(TM) Cricket 07")
```

---

## 5. Audit Findings & Overall Readiness Assessment

| Requirement | Expected File | Local Status | Severity | Impact on Project |
| :--- | :--- | :---: | :---: | :--- |
| **Game Executable** | `Cricket07.exe` | **MISSING** | **CRITICAL** | Cannot perform Ghidra analysis or x86-to-C translation. |
| **Primary Archive** | `data.gob` | **MISSING** | **CRITICAL** | Cannot load match models, textures, or core game data. |
| **Menu UI Archive** | `fe.big` | **MISSING** | **CRITICAL** | Cannot render authentic EA menus or UI sprites. |
| **Audio Archive** | `audio.big` | **MISSING** | **HIGH** | Sound bridge operational, but no authentic audio assets exist. |
| **Motion Archive** | `motion.big` | **MISSING** | **HIGH** | Match simulation animations cannot be accessed. |
| **Translation JSON**| `functions.json` | **MISSING** | **CRITICAL** | Translation input contract has not been generated. |
| **Translated C Code**| `recomp_*.c` | **MISSING** | **CRITICAL** | No translated game code exists to compile or link. |

### Overall Readiness: BLOCKED
Authentic game integration cannot proceed without legitimate, user-provided game files. Under strict adherence to Non-Negotiable Rules 7 and 12, no synthetic files have been fabricated and no proprietary EA assets will be acquired without user authorization.
