# CRICKET LEGACY — Phase C.2 Stage 3: CRT Startup and WinMain Trampoline Verification Report
**Document ID:** `CL-C2-STAGE3-VERIF-v1.0`  
**Classification:** Empirical Test & Android Device Execution Sign-Off  
**Target Environment:** Android 14 (API 34) AVD (`emulator-5554`)  
**Architecture:** x86_64 & ARM64-v8a Dual-ABI Parity  
**Status:** 8/8 TESTS PASSED (0 FAILED) — 100% SUCCESS  

---

## 1. Executive Summary

On **2026-11-10**, the **Phase C.2 Stage 3: CRT Startup and WinMain Trampoline** test harness was compiled into `libcricket_native_harness.so`, packaged into `cricket-legacy-debug.apk`, installed on Android 14 (`emulator-5554`), and executed via `MainActivity`.

All **8/8 empirical tests passed cleanly**. The dedicated game pthread launched with a 64 MB host stack, configured the guest CPU registers and Thread Information Block, pushed WinMain arguments, executed the clearly labeled `test_crt_winmain_program`, validated ABI semantics, and cleanly returned exit code `0xCAFE0007`.

---

## 2. Test Execution Matrix

| Test ID | Test Description | Measured Result | Expected Output | Status |
| :---: | :--- | :--- | :--- | :---: |
| **TEST 1** | Command Line Setup (`GetCommandLineA/W`) | `"\"C:\Cricket07\Cricket07.exe\""` at `0x10000010` | Valid guest string pointer with path | **PASS** |
| **TEST 2** | Startup Info Structure (`GetStartupInfoA`) | `cb = 68` bytes | Matches `sizeof(Win32StartupInfoA)` | **PASS** |
| **TEST 3** | Module Handle Resolution (`GetModuleHandleA`) | NULL -> `0x00400000`; kernel32 -> `0xF1010000`; non -> `0` | ImageBase for NULL; fake for known | **PASS** |
| **TEST 4** | Environment Block (`GetEnvironmentStrings`) | Block at `0x10000090`: `"PATH=C:\WINDOWS"` | Double-null terminated environment | **PASS** |
| **TEST 5** | Windows Version Profile (`GetVersionExA`) | Version 5.1 (Build 2600), Platform 2, "Service Pack 2" | Windows XP SP2 NT profile | **PASS** |
| **TEST 6** | Process Heap (`GetProcessHeap`, `HeapAlloc`) | Heap: `0x00010000`; alloc 512 bytes at `0x10000240` | Valid heap pointer and zeroed bytes | **PASS** |
| **TEST 7** | Standard Device Handles (`GetStdHandle`) | IN: `0xFFFFFFF6`, OUT: `0xFFFFFFF5`, ERR: `0xFFFFFFF4` | Correct standard handle IDs | **PASS** |
| **TEST 8** | WinMain Dedicated Thread Trampoline | Spawned 64MB pthread; verified ABI; ExitCode `0xCAFE0007` | Synthetic test program executed | **PASS** |

---

## 3. Live Android System Logcat Capture

```text
11-10 08:03:24.140 13935 13935 I CricketNativeHarness: Executing Phase C.2 Stage 3 CRT Startup and WinMain Trampoline Suite...
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: ==================================================================
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: PHASE C.2 — STAGE 3: CRT STARTUP & WINMAIN TRAMPOLINE VERIFICATION
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: MSVC CRT Initialization, Command Line & Game Thread Trampoline
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: ==================================================================
11-10 08:03:24.140 13935 13935 I CricketCRT: CRT Startup Subsystem Initialized. CmdLine: "C:\Cricket07\Cricket07.exe", ImageBase: 0x00400000
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: [TEST 1] Testing Win32 Command Line Setup (GetCommandLineA/W)...
11-10 08:03:24.140 13935 13935 I Stage3CRTTests:   GetCommandLineA: '"C:\Cricket07\Cricket07.exe"' (Guest Address: 0x10000010)
11-10 08:03:24.140 13935 13935 I Stage3CRTTests:   PASS: Test 1 Command Line verified.
11-10 08:03:24.140 13935 13935 I Stage3CRTTests: [TEST 2] Testing Win32 Startup Info (GetStartupInfoA)...
11-10 08:03:24.140 13935 13935 I Stage3CRTTests:   GetStartupInfoA cb: 68 bytes (Expected 68)
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   PASS: Test 2 Startup Info verified.
11-10 08:03:24.141 13935 13935 I Stage3CRTTests: [TEST 3] Testing Module Handle Resolution (GetModuleHandleA)...
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   GetModuleHandleA(NULL) -> 0x00400000
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   GetModuleHandleA('kernel32.dll') -> 0xF1010000
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   GetModuleHandleA('nonexistent.dll') -> 0x00000000
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   PASS: Test 3 Module Handles verified.
11-10 08:03:24.141 13935 13935 I Stage3CRTTests: [TEST 4] Testing Win32 Environment Block (GetEnvironmentStrings)...
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   Environment Block at 0x10000090: 'PATH=C:\WINDOWS'
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   PASS: Test 4 Environment Strings verified.
11-10 08:03:24.141 13935 13935 I Stage3CRTTests: [TEST 5] Testing Win32 OS Version (GetVersionExA)...
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   Windows Version: 5.1 (Build 2600), PlatformId: 2, CSD: 'Service Pack 2'
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   PASS: Test 5 Version Info verified.
11-10 08:03:24.141 13935 13935 I Stage3CRTTests: [TEST 6] Testing Process Heap & Dynamic Allocations (GetProcessHeap, HeapAlloc)...
11-10 08:03:24.141 13935 13935 I Stage3CRTTests:   GetProcessHeap() -> 0x00010000
11-10 08:03:24.142 13935 13935 I Stage3CRTTests:   HeapAlloc(512) -> 0x10000240
11-10 08:03:24.142 13935 13935 I Stage3CRTTests:   PASS: Test 6 Process Heap verified.
11-10 08:03:24.142 13935 13935 I Stage3CRTTests: [TEST 7] Testing Standard Device Handles (GetStdHandle)...
11-10 08:03:24.142 13935 13935 I Stage3CRTTests:   Std Handles: IN=0xFFFFFFF6, OUT=0xFFFFFFF5, ERR=0xFFFFFFF4
11-10 08:03:24.142 13935 13935 I Stage3CRTTests:   PASS: Test 7 Standard Handles verified.
11-10 08:03:24.142 13935 13935 I Stage3CRTTests: [TEST 8] Testing WinMain Dedicated Thread Trampoline with Synthetic Test Program...
11-10 08:03:24.142 13935 13935 I Stage3CRTTests:   NOTE: Executing clearly labeled 'test_crt_winmain_program' (NOT actual Cricket 07 code).
11-10 08:03:24.142 13935 13957 I CricketCRT: [GameThread] Worker thread started with 64MB host stack.
11-10 08:03:24.143 13935 13957 I CricketCRT: [GameThread] Stack prepared: esp=0x002FFFDC. Entering entry trampoline...
11-10 08:03:24.143 13935 13957 I CricketCRT:   [SYNTHETIC TEST PROGRAM] WinMain Entry reached!
11-10 08:03:24.143 13935 13957 I CricketCRT:     ARG(0) hInstance     : 0x00400000
11-10 08:03:24.143 13935 13957 I CricketCRT:     ARG(1) hPrevInstance : 0x00000000
11-10 08:03:24.143 13935 13957 I CricketCRT:     ARG(2) lpCmdLine     : 0x10000010 ('"C:\Cricket07\Cricket07.exe"')
11-10 08:03:24.143 13935 13957 I CricketCRT:     ARG(3) nCmdShow      : 10
11-10 08:03:24.143 13935 13957 I CricketCRT:   [SYNTHETIC TEST PROGRAM] All ABI and runtime checks passed! Returning 0xCAFE0007.
11-10 08:03:24.143 13935 13957 I CricketCRT: [GameThread] Trampoline execution completed. ExitCode: 0xCAFE0007 (-889323513)
11-10 08:03:24.143 13935 13935 I Stage3CRTTests:   WinMain execution completed with ExitCode: 0xCAFE0007
11-10 08:03:24.143 13935 13935 I Stage3CRTTests:   PASS: Test 8 WinMain Launch Trampoline verified.
11-10 08:03:24.144 13935 13935 I Stage3CRTTests: ==================================================================
11-10 08:03:24.144 13935 13935 I Stage3CRTTests: STAGE 3 TEST SUMMARY: 8 / 8 TESTS PASSED (Failed: 0)
11-10 08:03:24.144 13935 13935 I Stage3CRTTests: ==================================================================
11-10 08:03:24.144 13935 13935 I CricketNativeHarness: Phase C.2 Stage 3 CRT Startup and WinMain Trampoline Suite PASSED successfully!
```

---

## 4. Multi-ABI Parity

- **`arm64-v8a`:** Built cleanly (`libcricket_native_harness.so`, 536 KB) with Android NDK 27.1. Zero warnings, zero errors.
- **`x86_64`:** Built cleanly (`libcricket_native_harness.so`, 510 KB) with Android NDK 27.1. Zero warnings, zero errors.

---

## 5. Architectural Readiness for Stage 4

With the CRT startup and WinMain launch trampoline validated on Android, the host harness is prepared for:  
👉 **Stage 4: Engine Main Operating Loop** (Win32 message dispatch pump `PeekMessageA`/`DispatchMessageA`, window procedure dispatch, frame pacing, and Android lifecycle suspend/resume hooks).
