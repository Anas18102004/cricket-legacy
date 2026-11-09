# CRICKET LEGACY — Phase B.5: Build & Device Execution Guide
**Document ID:** `CL-B5-BUILD-v1.0`  
**Classification:** Build Engineering & Deployment Guide  

---

## 1. Environment & Prerequisites

The following tools are installed and verified on the local workstation:
- **Android SDK:** `C:\Users\Mohammad Anas\AppData\Local\Android\Sdk`
- **Android NDK:** Version `27.1.12297006`
- **CMake:** Version `3.22.1` (`C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\cmake\3.22.1\bin\cmake.exe`)
- **Ninja:** `C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\cmake\3.22.1\bin\ninja.exe`
- **Java JDK:** Microsoft OpenJDK 17 (`C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot\bin`)
- **ADB:** `C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe`

---

## 2. Compiling the Native Shared Libraries

### Step 2.1: Compile for ARM64 Physical Devices (`arm64-v8a`)
Open PowerShell and run:
```powershell
$cmake = "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\cmake\3.22.1\bin\cmake.exe"
$ninja = "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\cmake\3.22.1\bin\ninja.exe"
$ndkToolchain = "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\ndk\27.1.12297006\build\cmake\android.toolchain.cmake"
$buildDir = "d:\cricket07 mobile app\android-native-prototype\build-arm64"
$srcDir = "d:\cricket07 mobile app\android-native-prototype"

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Set-Location $buildDir

& $cmake -G "Ninja" `
    -DCMAKE_MAKE_PROGRAM="$ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$ndkToolchain" `
    -DANDROID_ABI="arm64-v8a" `
    -DANDROID_PLATFORM="android-26" `
    -DCMAKE_BUILD_TYPE="Release" `
    "$srcDir"

& $ninja -v
```

### Step 2.2: Compile for Local Emulator (`x86_64`)
```powershell
$buildDirX64 = "d:\cricket07 mobile app\android-native-prototype\build-x86_64"

New-Item -ItemType Directory -Path $buildDirX64 -Force | Out-Null
Set-Location $buildDirX64

& $cmake -G "Ninja" `
    -DCMAKE_MAKE_PROGRAM="$ninja" `
    -DCMAKE_TOOLCHAIN_FILE="$ndkToolchain" `
    -DANDROID_ABI="x86_64" `
    -DANDROID_PLATFORM="android-26" `
    -DCMAKE_BUILD_TYPE="Release" `
    "$srcDir"

& $ninja -v
```

### Step 2.3: Stage Native Binaries into Android App
```powershell
$arm64Dest = "d:\cricket07 mobile app\android-native-prototype\android-app\app\src\main\jniLibs\arm64-v8a"
$x64Dest = "d:\cricket07 mobile app\android-native-prototype\android-app\app\src\main\jniLibs\x86_64"

New-Item -ItemType Directory -Path $arm64Dest -Force | Out-Null
New-Item -ItemType Directory -Path $x64Dest -Force | Out-Null

Copy-Item "d:\cricket07 mobile app\android-native-prototype\build-arm64\libcricket_native_harness.so" "$arm64Dest\" -Force
Copy-Item "d:\cricket07 mobile app\android-native-prototype\build-x86_64\libcricket_native_harness.so" "$x64Dest\" -Force
```

---

## 3. Launching on Device or Emulator

### Option A: Testing on Connected Physical Android Device
1. Connect Android smartphone via USB with **USB Debugging** enabled.
2. Verify connection:
   ```powershell
   & "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" devices
   ```
3. Install and run APK:
   ```powershell
   & "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" install -r app-debug.apk
   & "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" shell am start -n org.cricketlegacy/.MainActivity
   ```

### Option B: Testing on Local AVD Emulator
1. Boot the installed Android 14 AVD emulator:
   ```powershell
   & "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\emulator\emulator.exe" -avd TrueCost_Test_AVD_API34 -gpu host
   ```
2. Wait for boot completion:
   ```powershell
   & "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" wait-for-device
   ```
3. Sideload and launch as above.

---

## 4. Capturing Live Diagnostics

Monitor native telemetry in real time:
```powershell
& "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" logcat -c
& "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platform-tools\adb.exe" logcat -s `
    CricketNativeHarness:I `
    CricketMemory:I `
    CricketGraphics:I `
    CricketPhysics:I `
    CricketLegacyApp:I
```

### Expected Log Output:
```
I/CricketLegacyApp: Successfully loaded native library: libcricket_native_harness.so
I/CricketMemory: Strategy 1 SUCCESS: Mapped directly in lower 32-bit memory at 0x10000
I/CricketMemory: Memory Arena 32-bit translation check PASSED (Offset 0x0045A120 = 0x12345678)
I/CricketPhysics: Starting ARM64 Ballistic Validation Test: 140 km/h release...
I/CricketPhysics: Pitch impact registered! New Vy: 4.82 m/s, Vz: -34.12 m/s
I/CricketPhysics: Ball reached batsman crease at step 54: Speed=128.4 km/h
I/CricketPhysics: ARM64 Ballistic Validation Test PASSED successfully!
I/CricketLegacyApp: Audio test playback tone successfully triggered!
I/CricketGraphics: Vulkan instance created successfully!
I/CricketGraphics: Primary GPU Detected: Adreno / Mali / Iris
I/CricketGraphics: Graphics initialized successfully on surface.
```
