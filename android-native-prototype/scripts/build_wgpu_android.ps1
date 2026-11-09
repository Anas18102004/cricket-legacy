# Reproducible build script for wgpu-native on Android (ARM64-v8a & x86_64)
# Configures NDK Clang 18 and Rust toolchain with libclang bindgen bindings.

$ErrorActionPreference = "Stop"

$NDK_ROOT = "C:/Users/MOHAMM~1/AppData/Local/Android/Sdk/ndk/27.1.12297006"
$CLANG_BIN = "$NDK_ROOT/toolchains/llvm/prebuilt/windows-x86_64/bin"
$SYSROOT = "$NDK_ROOT/toolchains/llvm/prebuilt/windows-x86_64/sysroot"

$env:LIBCLANG_PATH = "C:\Users\MOHAMM~1\AppData\Local\Programs\Python\Python310\lib\site-packages\clang\native"
$env:BINDGEN_EXTRA_CLANG_ARGS = "-I$SYSROOT/usr/include -I$NDK_ROOT/toolchains/llvm/prebuilt/windows-x86_64/lib/clang/18/include"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "CRICKET LEGACY: Compiling wgpu-native for Android Targets" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$WGPU_DIR = "$PSScriptRoot/../../third_party/wgpu"
Push-Location $WGPU_DIR

# 1. Build x86_64 for Android Emulator
Write-Host "[1/2] Building wgpu-native for x86_64-linux-android..." -ForegroundColor Green
cargo build --target x86_64-linux-android --release

# 2. Build aarch64 for Android Physical Hardware (ARM64-v8a)
Write-Host "[2/2] Building wgpu-native for aarch64-linux-android..." -ForegroundColor Green
cargo build --target aarch64-linux-android --release

Pop-Location

# 3. Stage artifacts to Android prototype jniLibs
$JNILIBS = "$PSScriptRoot/../android-app/app/src/main/jniLibs"
New-Item -ItemType Directory -Path "$JNILIBS/x86_64", "$JNILIBS/arm64-v8a" -Force | Out-Null

Copy-Item -Path "$WGPU_DIR/target/x86_64-linux-android/release/libwgpu_native.so" -Destination "$JNILIBS/x86_64/libwgpu_native.so" -Force
Copy-Item -Path "$WGPU_DIR/target/aarch64-linux-android/release/libwgpu_native.so" -Destination "$JNILIBS/arm64-v8a/libwgpu_native.so" -Force

Write-Host "Successfully built and staged libwgpu_native.so for both ABIs!" -ForegroundColor Cyan
