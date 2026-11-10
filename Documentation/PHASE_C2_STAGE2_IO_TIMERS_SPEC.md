# CRICKET LEGACY — Phase C.2 Stage 2: File I/O and Timers Specification
**Document ID:** `CL-C2-STAGE2-SPEC-v1.0`  
**Classification:** System Architecture & Win32 Compatibility Layer  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** APPROVED & EMPIRICALLY VERIFIED  

---

## 1. Executive Overview

In EA SPORTS Cricket 07 (Windows x86, 2006), the game engine relies heavily on Win32 `kernel32.dll` and `winmm.dll` for:
1. **Asset Loading:** Synchronous file reads of compiled scripts, stadium geometry, and `.big` composite archives (`data.gob`, `speech.big`).
2. **User Data Persistence:** R/W operations for user configuration, player profiles, tournament saves, and controller mappings under `My Documents\EA SPORTS(TM) Cricket 07`.
3. **Pacing and Physics Synchronization:** Microsecond-accurate performance counters (`QueryPerformanceCounter`) and millisecond multimedia timers (`timeGetTime`) to drive ball physics, camera interpolation, and frame pacing.

Phase C.2 Stage 2 implements and validates this entire I/O and timing infrastructure on native Android, bridging Windows semantics onto the Android Bionic C runtime.

---

## 2. File I/O Architectural Model

```
                    ┌──────────────────────────────────────────────┐
                    │      Recompiled Cricket 07 Game Engine       │
                    │         (32-bit Win32 Call Site)             │
                    └──────────────────────┬───────────────────────┘
                                           │
                        stdcall: w_CreateFileA / w_ReadFile
                                           │
                                           ▼
                    ┌──────────────────────────────────────────────┐
                    │         Path Normalization & Mapping         │
                    │         (src/game_file_io.c: map_path)       │
                    ├──────────────────────────────────────────────┤
                    │ • C:\Cricket07\...   -> /data/data/.../game  │
                    │ • My Documents\...   -> /data/data/.../user  │
                    │ • Case-Insensitive Matching (opendir/readdir)│
                    │ • Parent Directory Auto-Creation (mkdir -p) │
                    └──────────────────────┬───────────────────────┘
                                           │
                                           ▼
                    ┌──────────────────────────────────────────────┐
                    │            Win32 Handle Manager              │
                    │    (H_FILE, H_FIND, H_MAPPING: g_handles)    │
                    ├──────────────────────────────────────────────┤
                    │ • Valid handles: 0x00010001 - 0x00010400     │
                    │ • POSIX fd binding & lifecycle tracking      │
                    │ • Thread-safe mutex serialization            │
                    └──────────────────────┬───────────────────────┘
                                           │
                                           ▼
                    ┌──────────────────────────────────────────────┐
                    │           Linux / Android Kernel             │
                    │    (open, read, write, lseek, fstat, close)  │
                    └──────────────────────────────────────────────┘
```

### 2.1 Handle Management Table
- **Handle Base:** `0x00010000u` (Indices 1 to 1024, preventing NULL or low integer conflicts).
- **Structure:** `WinHandle` tracks handle type (`H_FILE`), file descriptor `fd`, canonical host path, and mutex protection.
- **Closed Handle Protection:** Closing a handle invalidates the slot and zeroes the `fd`. Any subsequent operation immediately returns `ERROR_INVALID_HANDLE` (`0x00000006`).

### 2.2 Path Mapping Engine (`map_path`)
On Windows, file paths are case-insensitive and utilize backslashes (`\`). On Android Linux filesystems (`ext4`/`f2fs`), paths are strictly case-sensitive and utilize forward slashes (`/`).
The `map_path()` function resolves this incompatibility through a 3-step pipeline:
1. **Normalization:** Converts relative paths to absolute using `g_cwd` (`C:\Cricket07`), replaces all `/` with `\`.
2. **Partitioning:**
   - Paths starting with `PROFILE_GUEST_ROOT` (`C:\Cricket07`) map to `g_game_root` (`/data/data/org.cricketlegacy/files/game`).
   - All other paths (saves, profiles, config) map to `g_user_root` (`/data/data/org.cricketlegacy/files/userdata`).
3. **Case-Insensitive Resolution (`find_ci`):**
   - For each directory component, `find_ci` executes `opendir()` and scans directory entries using `strcasecmp()`.
   - If an entry exists with different casing (e.g. on-disk `FeStReAm` vs requested `festream`), the real on-disk casing is substituted into the path.
   - If writing a new file, `ensure_parent_dirs()` automatically constructs missing intermediate directories.

---

## 3. High-Resolution Timers & Clock Synchronization

### 3.1 Performance Frequency and Counter
- **Frequency:** Defined as `1,000,000,000ULL` Hz (1 GHz), yielding 1 nanosecond per tick.
- **Counter:** Sampled directly from POSIX `CLOCK_MONOTONIC`.
- **Monotonicity:** Guarantees strict monotonic advancement regardless of system wall-clock adjustments (NTP, daylight saving time).

### 3.2 WinMM Multimedia Timers (`timeGetTime`)
- The original game engine casts `timeGetTime()` and `GetTickCount()` values to single-precision 32-bit floats (`unaligned_f32`).
- A standard raw Unix timestamp (~1.7 billion seconds) exceeds the 24-bit mantissa of an IEEE 754 float, which degrades resolution to ~128 ms chunks and breaks game timer loops.
- **Solution:** `timer_host_seconds()` offsets the monotonic clock by `+60.0` seconds from application launch. The resulting float values retain full sub-millisecond precision.

### 3.3 Windows Epoch FILETIME (`host_time_100ns`)
- Computes 100-nanosecond intervals elapsed since **January 1, 1601 (UTC)**.
- Base offset: `11,644,473,600` seconds added to Unix `CLOCK_REALTIME`.

---

## 4. EA Sports `.big` Archive Lazy Demand-Paging

EA Sports Cricket 07 uses monolithic archive files (`data.gob` at 325 MB, motion archives at 64 MB). Loading them fully into physical mobile memory would trigger the Android Low Memory Killer (LMK).

### 4.1 Lazy View Architecture
- **Chunk Size:** 64 KB (`VIEW_CHUNK = 0x10000u`, 65,536 bytes).
- **Virtual Reservation:** Allocated in guest virtual address space (`0x60000000`–`0xC0000000`) via `guest_valloc()`.
- **Bitmask Tracking:** An atomic bitmask (`LazyView.filled`) tracks each 64 KB chunk state.
- **Entry Hook (`hook_copy_source`):**
  When the game's internal `memcpy` function (`0x652a00`) is called:
  1. The entry hook intercepts `src` address and `count`.
  2. If `src` resides inside an active lazy view, `view_fill()` identifies unpaged chunks.
  3. `pread()` loads only the referenced 64 KB chunks directly from storage into RAM.
  4. The chunk bits are atomically set to filled, and the memory copy proceeds normally.

---

## 5. Architectural Compliance Verification Matrix

| Component | Android C Implementation | Upstream Compatibility | Status |
| :--- | :--- | :--- | :--- |
| **Win32 Handles** | [`game_file_io.c:38`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_file_io.c#L38) | `host/kernel32.c:817` | **VERIFIED** |
| **Path Normalization** | [`game_file_io.c:134`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_file_io.c#L134) | `host/core.c:403` | **VERIFIED** |
| **Case-Insensitive Resolution** | [`game_file_io.c:98`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_file_io.c#L98) | `host/core.c:443` | **VERIFIED** |
| **Performance Counter (QPC)** | [`game_timers.c:46`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_timers.c#L46) | `host/kernel32.c:354` | **VERIFIED** |
| **WinMM Timer (timeGetTime)** | [`game_timers.c:53`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_timers.c#L53) | `host/core.c:488` | **VERIFIED** |
| **Lazy Views Demand Paging** | [`game_lazy_views.c:140`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_lazy_views.c#L140) | `host/core.c:174` | **VERIFIED** |
