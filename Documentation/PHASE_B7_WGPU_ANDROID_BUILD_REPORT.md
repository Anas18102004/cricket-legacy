# Phase B.7: wgpu-native Cross-Compilation & Android Shared Library Report

**Project:** Cricket Legacy (EA SPORTS Cricket 07 Mobile Port)  
**Milestone:** Phase B.7 Native Validation  
**Date:** November 2026  
**Status:** **SUCCESSFULLY COMPILED & LINKED**  
**Component:** `wgpu-native` (Pinned Release `v22.1.0.5`)  

---

## 1. Executive Summary

To deliver native hardware-accelerated 3D rendering for Cricket Legacy on Android without introducing Unity, third-party wrappers, or custom C++ abstractions, we successfully cross-compiled `wgpu-native` directly from source for:
1. `aarch64-linux-android` (Targeting physical ARM64 mobile hardware)
2. `x86_64-linux-android` (Targeting development emulators running Android 14 API 34)

The build utilizes Android NDK 27.1 (Clang 18.0.2), Rust toolchain 1.95.0, and bindgen with native Clang bindings.

---

## 2. Source Code & Version Pinning

- **Repository:** `https://github.com/gfx-rs/wgpu-native`
- **Pinned Tag:** `v22.1.0.5`
- **Local Location:** `third_party/wgpu`
- **Submodules Initialized:** `git submodule update --init --recursive`
- **Features Enabled:** Standard `wgpu-native` shared library features (`cdylib`, Vulkan backend on Android).

---

## 3. Toolchain & Environment Configuration

### 3.1 Toolchain Matrix
| Component | Path / Version |
|---|---|
| **Host OS** | Windows 11 x86_64 |
| **Rust Toolchain** | Cargo 1.95.0 (stable-x86_64-pc-windows-msvc) |
| **Android NDK** | NDK 27.1.12297006 (`C:\Users\MOHAMM~1\AppData\Local\Android\Sdk\ndk\27.1.12297006`) |
| **NDK Clang** | Clang 18.0.2 (`bin\clang.exe` / `bin\aarch64-linux-android34-clang.cmd`) |
| **NDK sysroot** | `toolchains\llvm\prebuilt\windows-x86_64\sysroot` |
| **libclang** | LLVM Clang 18.1.1 (`pip install libclang`) |

### 3.2 Solving Windows Toolchain Challenges

#### Challenge A: Bindgen Clang Resolution
Rust's `bindgen` crate requires `libclang.dll` to parse C headers (`webgpu.h`). On Windows, standard NDK LLVM installations do not always expose `libclang.dll` directly to the Windows PATH.
*Solution:* Installed Python `libclang` 18.1.1 via pip, configuring:
```powershell
$env:LIBCLANG_PATH = "C:\Users\MOHAMM~1\AppData\Local\Programs\Python\Python310\lib\site-packages\clang\native"
```

#### Challenge B: Whitespace in User Profile Paths
The user profile directory path contains spaces (`C:\Users\Mohammad Anas\...`). Cargo build scripts and Clang argument forwarders in `bindgen` split arguments with spaces, producing errors such as `'Anas/AppData/...: linker input unused'`.
*Solution:* Canonicalized all NDK, SDK, and sysroot paths to Windows 8.3 short paths (`C:\Users\MOHAMM~1\...`).

#### Challenge C: Cross-Compilation Target Config (`.cargo/config.toml`)
We generated `third_party/wgpu/.cargo/config.toml` configuring explicit cross-compile toolchains:

```toml
[target.aarch64-linux-android]
ar = "C:/Users/MOHAMM~1/AppData/Local/Android/Sdk/ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar.exe"
linker = "C:/Users/MOHAMM~1/AppData/Local/Android/Sdk/ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android34-clang.cmd"

[target.x86_64-linux-android]
ar = "C:/Users/MOHAMM~1/AppData/Local/Android/Sdk/ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar.exe"
linker = "C:/Users/MOHAMM~1/AppData/Local/Android/Sdk/ndk/27.1.12297006/toolchains/llvm/prebuilt/windows-x86_64/bin/x86_64-linux-android34-clang.cmd"
```

---

## 4. Compilation Results & ELF Telemetry

Both shared libraries were compiled with `cargo build --release --target <target>` and inspected using `llvm-readelf`.

### 4.1 Target: `aarch64-linux-android`
- **Output Artifact:** `target/aarch64-linux-android/release/libwgpu_native.so`
- **Staged Location:** `android-native-prototype/android-app/app/src/main/jniLibs/arm64-v8a/libwgpu_native.so`
- **File Size:** 9,522,544 bytes (9.08 MB)
- **ELF Architecture:** `ELF64`, `AArch64`, `Little-endian`, `Version 1 (current)`
- **Dynamic Linker Dependencies (`DT_NEEDED`):**
  - `liblog.so`
  - `libm.so`
  - `libdl.so`
  - `libc.so`
- **Android Vulkan Support:** Full Vulkan 1.1 / 1.2 / 1.3 driver entrypoints bound dynamically via `dlopen("libvulkan.so")`.

### 4.2 Target: `x86_64-linux-android`
- **Output Artifact:** `target/x86_64-linux-android/release/libwgpu_native.so`
- **Staged Location:** `android-native-prototype/android-app/app/src/main/jniLibs/x86_64/libwgpu_native.so`
- **File Size:** 9,219,368 bytes (8.79 MB)
- **ELF Architecture:** `ELF64`, `Advanced Micro Devices X86-64`, `Little-endian`, `Version 1 (current)`
- **Dynamic Linker Dependencies (`DT_NEEDED`):**
  - `liblog.so`
  - `libm.so`
  - `libdl.so`
  - `libc.so`

---

## 5. Header Export & C ABI Compatibility

The C ABI headers were extracted from `third_party/wgpu/ffi` and installed into the Android native harness include directory:
- `android-native-prototype/include/webgpu/webgpu.h` (Official WebGPU C standard header)
- `android-native-prototype/include/webgpu/wgpu.h` (`wgpu-native` specific extensions)

Both headers are configured for ISO C11 compilation and exported with C ABI linkage:
```c
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
```

---

## 6. Dynamic Linker SONAME Rules & Android Integration

When building shared libraries with Cargo cdylib, Cargo does not automatically append `-Wl,-soname,libwgpu_native.so` on some targets. If CMake links against the library using an absolute or relative filepath target, the Clang driver writes the literal path into `DT_NEEDED` of the consuming library (`libcricket_native_harness.so`), causing Android's Bionic dynamic linker to fail at runtime with `dlopen failed: library "../.../libwgpu_native.so" not found`.

### Resolution in `CMakeLists.txt`
In [CMakeLists.txt](file:///d:/cricket07%20mobile%20app/android-native-prototype/CMakeLists.txt), we resolved this by utilizing directory search flags:
```cmake
target_link_directories(cricket_native_harness PRIVATE "${WGPU_LIB_DIR}")
target_link_libraries(cricket_native_harness
    wgpu_native
    android
    log
    vulkan
)
```
Verification with `llvm-readelf -d libcricket_native_harness.so` confirmed:
```
0x0000000000000001 (NEEDED)             Shared library: [libwgpu_native.so]
0x0000000000000001 (NEEDED)             Shared library: [libandroid.so]
0x0000000000000001 (NEEDED)             Shared library: [liblog.so]
0x0000000000000001 (NEEDED)             Shared library: [libvulkan.so]
0x0000000000000001 (NEEDED)             Shared library: [libc.so]
```
This guarantees 100% standard Android dynamic linker resolution when loaded via `System.loadLibrary("wgpu_native")` and `System.loadLibrary("cricket_native_harness")`.

---

## 7. Automated Build Script

To ensure complete build reproducibility across developer environments, we committed:
[android-native-prototype/scripts/build_wgpu_android.ps1](file:///d:/cricket07%20mobile%20app/android-native-prototype/scripts/build_wgpu_android.ps1)

This script automates:
1. Environment detection (Rust, NDK, Python libclang).
2. Path normalization to 8.3 short format.
3. `.cargo/config.toml` target configuration.
4. Compilation for `x86_64-linux-android` and `aarch64-linux-android`.
5. Automatic staging of compiled `.so` files into `jniLibs/{x86_64,arm64-v8a}`.
6. Export of `webgpu.h` and `wgpu.h` C headers.
