# Codex Phase G Build Validation

**Commit:** `4dc5efaad8b3b306f6061ff742fb7e9d126b3ec4` (`phase-g-mobile-control-system`)  
**Working tree at start:** clean. **Audit changes:** reports and new evidence only.

## Results

| Check | Status | Evidence |
|---|---|---|
| Android APK packaging | PASS | `android-native-prototype/scripts/build_apk.ps1` exited successfully on 2026-11-10. APK: `android-native-prototype/android-app/app/build/outputs/apk/debug/cricket-legacy-debug.apk` (128,295,646 bytes). |
| Native CMake target, x86_64 | PASS | `cmake --build android-native-prototype/build-x86_64 --target cricket_native_harness -j 4`; exit 0, Ninja reported no work. |
| Native CMake target, ARM64 | PASS | Same command against `build-arm64`; exit 0, Ninja reported no work. This verifies configured builds, not a fresh compiler invocation. |
| Packaged ABI folders | PASS | `aapt dump badging/list` reports `arm64-v8a` and `x86_64`, including `libcricket_native_harness.so` and `libwgpu_native.so`. |
| Install on x86_64 emulator | PASS | `adb install -r` succeeded on `sdk_gphone64_x86_64` (`emulator-5554`). |
| ARM64 runtime | NOT TESTED | No ARM64 device/emulator available. APK presence is not runtime validation. |
| Physical-device runtime | NOT TESTED | `adb devices -l` showed only the x86_64 emulator. |

## Important build limitation

The APK script compiles Java, dexes it, and packages native libraries already present in `jniLibs`; it does not compile the C/C++ native libraries. The CMake targets were already up to date. Therefore this APK rebuild does not independently establish that the packaged native libraries were freshly built from this commit.

## Environment

Windows, Android SDK at `%ANDROID_SDK_ROOT%`, NDK `27.1.12297006`, CMake `3.22.1`, Java 17, emulator API/device details `sdk_gphone64_x86_64`. `adb devices -l` listed one emulator and no physical device. The APK contains two ABIs, but only x86_64 was installed and launched.
