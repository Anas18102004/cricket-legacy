# CRICKET LEGACY — Native Android Architecture Assessment & Implementation Blueprint
**Document ID:** `CL-NDK-ARCH-v1.0`  
**Target:** Android Native Proof-of-Concept (ARM64-v8a)  
**Baseline Stack:** Android NDK (r27+), Clang 18, CMake 3.22+, SDL2, wgpu-native (Vulkan backend)  

---

## 1. Subsystem Architecture & Code Reuse Map

The original `cricket07-browser` project successfully decouples the game engine into two distinct software layers:
1. **The Core Recompiled Game (`recompiled/`):** 31,548 translated C functions (~1.3M lines of code) implementing game rules, ball physics, character animations, player AI, collision math, and match state transitions.
2. **The Host Compatibility Layer (`host/`):** Replaces legacy Windows 32-bit OS APIs (Direct3D 8, DirectSound, DirectInput, Win32 File I/O, `VirtualAlloc`, threads, and timers).

```
+---------------------------------------------------------------------------------+
|                       RECOMPILED CRICKET 07 ENGINE (C CODE)                     |
|    Physics * Batting * Bowling * AI * Match Rules * Animations * Scorecards     |
+---------------------------------------------------------------------------------+
                                       |
+---------------------------------------------------------------------------------+
|                   HOST COMPATIBILITY INTERFACE (host/)                          |
+---------------------------------------------------------------------------------+
            |                       |                       |             |
+-----------------------+ +--------------------+ +-------------------+ +---------+
|   Graphics Bridge     | |   Audio Bridge     | |   Input Bridge    | | Storage |
| Direct3D 8 -> WebGPU  | | DirectSound -> SDL | | DirectInput->Touch| | POSIX / |
|   (wgpu-native Vulkan)| |   (AAudio / SL)    | | (Gestures & Stick)| | Assets  |
+-----------------------+ +--------------------+ +-------------------+ +---------+
                                       |
+---------------------------------------------------------------------------------+
|                         SDL2 PLATFORM LAYER (ANDROID NDK)                       |
|   SDLActivity * ANativeWindow * Vulkan Surface * Multi-Touch * Sensor & Audio   |
+---------------------------------------------------------------------------------+
```

### Detailed Subsystem Reuse Map

| Subsystem | Original PC / Browser Implementation | Android Native Adaptation | Reusability % | Implementation Effort |
| :--- | :--- | :--- | :---: | :--- |
| **Game Simulation** | 31,548 translated C functions | Direct compilation via NDK Clang (`-O2 -fno-strict-aliasing`) | **100%** | Zero logic changes; compiler flag tuning only. |
| **Direct3D 8 Engine** | Emulated D3D8 Device issuing WebGPU API calls | Connects to `wgpu-native` C headers (`wgpu.h`) linking `libvulkan.so` | **95%** | Reuses all WGSL shaders and pipeline descriptors. |
| **Window & Surface** | HTML5 Canvas (`#canvas`) | `SDLActivity` $\to$ `ANativeWindow` $\to$ `SDL_Vulkan_CreateSurface` | **90%** | Standard SDL2 Android integration. |
| **Audio Pipeline** | Web Audio API (`AudioContext`) | `SDL_OpenAudioDevice` / `SDL_QueueAudio` (AAudio / OpenSL ES) | **85%** | Replace JS ring buffer with SDL audio callback. |
| **Input System** | DOM Events & HTML overlay D-pad | Native multi-touch gesture interpreter (`SDL_TouchFingerEvent`) | **40%** | Replace DOM clicks with ergonomic swipe/analog math. |
| **File / VFS** | Emscripten IDBFS / OPFS / IndexedDB | Standard POSIX `fopen`/`fread` + Android `AAssetManager` fallback | **80%** | Map game directory to `/data/data/<pkg>/files`. |
| **Virtual Memory** | 4GB WebAssembly Linear Memory | `mmap` anonymous virtual arena (`MAP_ANONYMOUS | MAP_PRIVATE`) | **85%** | Allocate 4GB virtual address space on 64-bit ARM. |
| **Timing & Clocks** | `emscripten_get_now` / `performance.now` | `clock_gettime(CLOCK_MONOTONIC)` with sub-microsecond precision | **95%** | Drop-in C replacement for Win32 `QueryPerformanceCounter`. |

---

## 2. Graphics Backend Evaluation: wgpu-native vs. Google Dawn vs. Direct Vulkan

The user correctly requested a rigorous comparison between our three graphics options:

```
[Option A: Existing wgpu-native]  -->  [Option B: Google Dawn]  -->  [Option C: Direct Vulkan]
```

### Comparative Evaluation Matrix

| Criterion | Option A: Existing `wgpu-native` | Option B: Google Dawn | Option C: Direct Vulkan / GLES3 |
| :--- | :--- | :--- | :--- |
| **Architecture** | Rust implementation of WebGPU C API | C++ implementation of WebGPU C API | Bare-metal C/C++ graphics API |
| **Android Status** | **Mature Vulkan backend** (`aarch64-linux-android`) | **Experimental / WIP** (per Google documentation) | **Native OS Standard** on Android 8+ |
| **D3D8 Shader Reusability** | **100% Direct** (Runs existing WGSL shaders) | **100% Direct** (Runs WGSL shaders via Tint) | **0%** (Requires transpiling WGSL to SPIR-V) |
| **API Compatibility** | Matches existing `cricket07-browser` calls | Matches WebGPU C header specification | Requires complete rewrite of D3D8 bridge |
| **Build Complexity** | Single precompiled static/shared library | Complex GN / Chromium build system dependency | Simple NDK linkage (`-lvulkan`) |
| **Memory & Binary Size** | $\approx 14\text{ MB}$ dynamic library | $\approx 22\text{ MB}$ dynamic library | $0\text{ MB}$ (Built into Android OS) |
| **Maintenance Burden** | Very Low (Preserves existing code) | High (Tracking Google Dawn tree changes) | Very High (Custom render pass state machine) |

### Graphics Backend Recommendation
**Retain Option A (`wgpu-native`) as the Primary Engine Backend.**  
- The existing browser desktop port *already works with `wgpu-native`*.
- `wgpu-native` has robust, production-validated `aarch64-linux-android` cross-compilation support mapping directly to Android's `libvulkan.so`.
- Rewriting the D3D8 translation layer to direct Vulkan would discard months of working graphics emulation for zero gameplay benefit.
- If and only if a device-specific Vulkan driver incompatibility arises on older chipsets, Google Dawn can be evaluated as an in-place swap using the exact same WebGPU C API header (`webgpu.h`).

---

## 3. Critical Memory & Threading Risk Assessment on 64-bit ARM

Transitioning 32-bit x86 game code to 64-bit ARM on Android introduces distinct architectural hazards that must be actively guarded against:

### 3.1 32-Bit Pointer Width vs. 64-Bit ARM (`sizeof(void*)`)
- **Hazard:** The original x86 game code stores pointers inside 32-bit integers (`uint32_t`) and frequently performs raw pointer arithmetic (e.g. `pPlayer = *(uint32_t*)(base + offset)`). On a 64-bit ARM OS, native pointers are 64 bits wide. If pointers are treated as native 64-bit addresses, truncating them to 32 bits causes immediate memory corruption (`SIGSEGV`).
- **Mitigation in Static Recompiler:** The static recompilation pipeline in `retro-port` handles this via **Base Pointer Offsetting**:
  $$p_{\text{native}} = (\text{uint8\_t}*)g\_mem\_base + (\text{uint32\_t})addr_{x86}$$
  On 64-bit Android, the application allocates a contiguous 4GB virtual memory arena using `mmap`:
  ```c
  void* g_mem_base = mmap(NULL, 0x100000000ULL, PROT_READ | PROT_WRITE, 
                          MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
  ```
  Using `MAP_NORESERVE` ensures that virtual memory pages are only backed by physical RAM as they are actually touched by the game, preventing the operating system from terminating the app due to upfront memory reservation.

### 3.2 Struct Packing & Memory Alignment (`#pragma pack(1)`)
- **Hazard:** x86 processors support unaligned memory access in hardware with negligible penalties. In contrast, ARM64 architectures will fault or incur significant microarchitectural penalties if 32-bit or 64-bit integers are dereferenced on unaligned memory boundaries (e.g., misaligned pointer casts in binary file loading).
- **Mitigation:**
  - Enforce `-fno-strict-aliasing` in CMake.
  - Utilize `memcpy` or unaligned access intrinsics (`__builtin_memcpy` / `__attribute__((packed, aligned(1)))`) when reading binary `.big` and `.viv` player/stadium archives.

### 3.3 Android RAM Budget & Heap Limits
- Target: Maximum physical memory footprint $\le 750\text{ MB}$ to guarantee stability across 4GB and 6GB Android devices.
- In the original PC release, textures and audio files were decompressed directly into memory. On mobile, we employ streaming audio and mipmap downscaling for background stadium meshes.

### 3.4 Deterministic Timing & Thread Synchronization
- The original PC game calls `QueryPerformanceCounter` to synchronize physics at 60Hz.
- On Android, this maps directly to `clock_gettime(CLOCK_MONOTONIC_RAW, &ts)`, which is unaffected by system clock adjustments or NTP syncs, maintaining frame-pacing determinism.

---

## 4. Platform-Independent vs. Platform-Dependent Breakdown

```
[PLATFORM-INDEPENDENT: 85% OF CODEBASE]
* Ball physics integration equations
* Batting contact zones & timing windows
* Bowling AI decision trees & line/length selection
* Fielding intercept geometry & throw routing
* Animation state machines & bone transforms
* Match scoring, overs, wickets, and statistics rules
* Direct3D 8 fixed-function state emulation & WGSL shaders

[ANDROID-SPECIFIC: 15% OF CODEBASE]
* SDLActivity lifecycle integration (SurfaceCreated, onResume, onPause)
* Android NDK AAssetManager / native storage directory routing
* Vulkan surface creation via ANativeWindow
* Multi-touch gesture classifier (Screen space -> Pitch aiming & Swipes)
* OpenSL ES / AAudio stream buffer playback
* Logcat telemetry bridging (__android_log_print)
```

---

## 5. Verification & Minimal Harness Plan

To guarantee architectural stability without premature complexity, we construct **Phase B: The Minimal Android Native Harness** in an isolated workspace: `android-native-prototype/`.

### Acceptance Criteria for Minimal Harness:
1. Compiles cleanly for `arm64-v8a` using NDK Clang 18 and CMake.
2. Initializes native window surface via `ANativeWindow`.
3. Validates GPU backend context creation (Vulkan / EGL fallback).
4. Processes multi-touch pointer events and measures touch-to-render latency.
5. Emits real-time frame rate, memory utilization, and lifecycle telemetry to Android Logcat.
