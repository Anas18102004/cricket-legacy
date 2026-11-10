# CRICKET LEGACY — Phase C.2 Stage 3: CRT Startup and WinMain Trampoline Specification
**Document ID:** `CL-C2-STAGE3-SPEC-v1.0`  
**Classification:** System Architecture & Win32 Execution Trampoline  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** APPROVED & EMPIRICALLY VERIFIED  

---

## 1. Executive Overview

When a 32-bit Win32 PE executable (such as EA SPORTS Cricket 07, MSVC 7.1/8.0, 2006) boots, control does not transfer immediately to gameplay code. Instead, the OS loader hands execution to the MSVC C Runtime (`CRTStartup`), which:
1. Queries the command line string via `GetCommandLineA()` or `GetCommandLineW()`.
2. Inspects startup parameters via `GetStartupInfoA(&si)`.
3. Obtains the process image base (`HINSTANCE`) via `GetModuleHandleA(NULL)` (`0x00400000`).
4. Queries the OS version via `GetVersionExA(&vi)` to determine Windows NT kernel features.
5. Initializes the process heap via `GetProcessHeap()` (`0x00010000`).
6. Configures floating point unit rounding and control registers (`fpu_cw = 0x027F`, `mxcsr = 0x1F80`).
7. Prepares the standard WinMain stdcall stack arguments:
   `int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)`
8. Calls `WinMain()` on a dedicated execution thread.

Phase C.2 Stage 3 implements this entire startup environment and trampoline on native Android, verified with a clearly labeled synthetic test program.

---

## 2. Trampoline Architecture & Stack Layout

```
                        ┌────────────────────────────────────────────────┐
                        │      Android Process (MainActivity / JNI)      │
                        └───────────────────────┬────────────────────────┘
                                                │
                                    crt_launch_winmain()
                                                │
                                                ▼
                        ┌────────────────────────────────────────────────┐
                        │            Dedicated Game Thread               │
                        │    (pthread with 64 MB host stack size)        │
                        └───────────────────────┬────────────────────────┘
                                                │
                                                ▼
                        ┌────────────────────────────────────────────────┐
                        │            CPU State Initialization            │
                        ├────────────────────────────────────────────────┤
                        │ • fs_base: 0x00010000 (Guest TIB)              │
                        │ • fpu_cw:  0x027F (x87 control word)           │
                        │ • mxcsr:   0x1F80 (SSE control word)           │
                        │ • esp:     0x002FFFF0 (Emulated stack top)     │
                        └───────────────────────┬────────────────────────┘
                                                │
                                                ▼
                        ┌────────────────────────────────────────────────┐
                        │           WinMain stdcall Stack Setup          │
                        ├────────────────────────────────────────────────┤
                        │ [esp + 16] = nCmdShow      (10 = SW_SHOWDEF)   │
                        │ [esp + 12] = lpCmdLine     (0x10000010)        │
                        │ [esp +  8] = hPrevInstance (0x00000000)        │
                        │ [esp +  4] = hInstance     (0x00400000)        │
                        │ [esp +  0] = Return Addr   (0xFFFFFFFE Sentinel)│
                        └───────────────────────┬────────────────────────┘
                                                │
                                                ▼
                        ┌────────────────────────────────────────────────┐
                        │     Synthetic Test Program / Game WinMain      │
                        │       (Pops 4 arguments via RET(4, code))      │
                        └────────────────────────────────────────────────┘
```

---

## 3. Emulated CRT APIs

### 3.1 `GetCommandLineA` and `GetCommandLineW`
- Populates guest memory with `"\"C:\Cricket07\Cricket07.exe\""`.
- Returns a 32-bit guest pointer directly readable by the game engine.
- Supports wide-char UTF-16 representation for internationalized modules.

### 3.2 `GetStartupInfoA`
- Clears and writes a 68-byte `STARTUPINFOA` structure (`cb = 68`).
- Conforms to standard Win32 MSVC expectations, preventing null-pointer dereferences in CRT initialization routines.

### 3.3 `GetModuleHandleA`
- `GetModuleHandleA(NULL)` returns `g_image_lo` (`0x00400000`), the image base.
- Queries for recognized DLLs (`kernel32.dll`, `user32.dll`, `d3d8.dll`) return synthetic module handles (`0xF1010000`–`0xF10A0000`).
- Unrecognized DLLs return `0` (`NULL`).

### 3.4 `GetVersionExA`
- Populates `OSVERSIONINFOA` with:
  - `dwMajorVersion = 5`
  - `dwMinorVersion = 1`
  - `dwBuildNumber = 2600`
  - `dwPlatformId = 2` (`VER_PLATFORM_WIN32_NT`)
  - `szCSDVersion = "Service Pack 2"`
- Matches the Windows XP SP2 profile that EA Sports Cricket 07 expects.

---

## 4. Synthetic Test Program Specification

To adhere strictly to the project rule that **no claim of running authentic game code is made before the real files arrive**, Stage 3 is validated using:
- **Identifier:** `test_crt_winmain_program` ([`src/game_crt_winmain.c:266`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L266))
- **Validation Scope:**
  1. Asserts `hInstance == 0x00400000`.
  2. Asserts `hPrevInstance == 0x00000000`.
  3. Asserts `lpCmdLine` contains `"Cricket07.exe"`.
  4. Asserts `nCmdShow == 10` (`SW_SHOWDEFAULT`).
  5. Performs guest heap allocation, write, read verification, and deallocation.
  6. Returns exit code `0xCAFE0007` via stdcall `RET(4, 0xCAFE0007)`.

---

## 5. Architectural Compliance Matrix

| Subsystem | Android C Implementation | Upstream Reference | Empirical Status |
| :--- | :--- | :--- | :---: |
| **Command Line** | [`game_crt_winmain.c:120`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L120) | `host/kernel32.c:82` | **VERIFIED** |
| **Startup Info** | [`game_crt_winmain.c:134`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L134) | `host/kernel32.c:98` | **VERIFIED** |
| **Module Handles** | [`game_crt_winmain.c:113`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L113) | `host/kernel32.c:48` | **VERIFIED** |
| **OS Version** | [`game_crt_winmain.c:150`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L150) | `host/kernel32.c:28` | **VERIFIED** |
| **Process Heap** | [`game_crt_winmain.c:162`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L162) | `host/kernel32.c:402` | **VERIFIED** |
| **WinMain Trampoline** | [`game_crt_winmain.c:220`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_crt_winmain.c#L220) | `host/main.c:54` | **VERIFIED** |
