# Automated APK Build and Signing Pipeline for Cricket Legacy
$ErrorActionPreference = "Stop"

$BUILD_TOOLS = "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\build-tools\35.0.0"
$PLATFORM_JAR = "C:\Users\Mohammad Anas\AppData\Local\Android\Sdk\platforms\android-36\android.jar"
$KEYSTORE = "$HOME\.android\debug.keystore"

$APP_DIR = "d:\cricket07 mobile app\android-native-prototype\android-app\app"
$SRC_DIR = "$APP_DIR\src\main"
$OUT_DIR = "$APP_DIR\build\outputs\apk\debug"

New-Item -ItemType Directory -Path "$APP_DIR\build\tmp", "$APP_DIR\build\classes", "$OUT_DIR" -Force | Out-Null

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "CRICKET LEGACY: Compiling and Packaging Android APK" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Compile Java source to bytecode
Write-Host "[1/5] Compiling Java source with javac..." -ForegroundColor Green
$javaFiles = (Get-ChildItem -Path "$SRC_DIR\java\org\cricketlegacy" -Filter "*.java").FullName
& javac -cp "$PLATFORM_JAR" -d "$APP_DIR\build\classes" $javaFiles

# 2. Dex bytecode with d8
Write-Host "[2/5] Converting classes to Dalvik Executable (classes.dex) with d8..." -ForegroundColor Green
$classes = (Get-ChildItem -Path "$APP_DIR\build\classes" -Recurse -Filter "*.class").FullName
& "$BUILD_TOOLS\d8.bat" --lib "$PLATFORM_JAR" --output "$APP_DIR\build\tmp" $classes

# 3. Create APK skeleton with aapt2
Write-Host "[3/5] Linking AndroidManifest.xml and generating APK skeleton with aapt2..." -ForegroundColor Green
& "$BUILD_TOOLS\aapt2.exe" link -o "$APP_DIR\build\tmp\skeleton.apk" `
    -I "$PLATFORM_JAR" `
    --manifest "$SRC_DIR\AndroidManifest.xml" `
    --target-sdk-version 34

# 4. Assemble APK containing classes.dex and native libraries (jniLibs)
Write-Host "[4/5] Adding classes.dex and native libraries (arm64-v8a & x86_64) to APK..." -ForegroundColor Green
Copy-Item "$APP_DIR\build\tmp\skeleton.apk" "$APP_DIR\build\tmp\unaligned.apk" -Force

# Create ZIP structure for classes.dex and lib/
$zipPath = "$APP_DIR\build\tmp\payload.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

# Use jar command to insert into unaligned.apk
Push-Location "$APP_DIR\build\tmp"
& jar -uf "unaligned.apk" "classes.dex"
Pop-Location

# Add native libraries under lib/
$tempLibDir = "$APP_DIR\build\tmp\lib"
New-Item -ItemType Directory -Path "$tempLibDir\arm64-v8a", "$tempLibDir\x86_64" -Force | Out-Null
Copy-Item "$SRC_DIR\jniLibs\arm64-v8a\*.so" "$tempLibDir\arm64-v8a\" -Force
Copy-Item "$SRC_DIR\jniLibs\x86_64\*.so" "$tempLibDir\x86_64\" -Force

Push-Location "$APP_DIR\build\tmp"
& jar -uf "unaligned.apk" "lib/arm64-v8a/libwgpu_native.so" "lib/arm64-v8a/libcricket_native_harness.so" "lib/x86_64/libwgpu_native.so" "lib/x86_64/libcricket_native_harness.so"
Pop-Location

# 5. Zipalign and sign APK
Write-Host "[5/5] Aligning and signing APK..." -ForegroundColor Green
$alignedApk = "$APP_DIR\build\tmp\aligned.apk"
if (Test-Path $alignedApk) { Remove-Item $alignedApk -Force }
& "$BUILD_TOOLS\zipalign.exe" -f -p 4 "$APP_DIR\build\tmp\unaligned.apk" "$alignedApk"

$finalApk = "$OUT_DIR\cricket-legacy-debug.apk"
if (Test-Path $finalApk) { Remove-Item $finalApk -Force }

& "$BUILD_TOOLS\apksigner.bat" sign `
    --ks "$KEYSTORE" `
    --ks-pass "pass:android" `
    --key-pass "pass:android" `
    --out "$finalApk" `
    "$alignedApk"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "APK BUILD SUCCESSFUL: $finalApk" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
