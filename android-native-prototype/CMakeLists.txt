cmake_minimum_required(VERSION 3.22.1)
project(cricket_native_harness C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
    -O1
    -Wall
    -fno-strict-aliasing
    -frounding-math
    -fPIC
    -w
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/upstream_d3d8
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    "D:/cricket07-browser-src/runtime"
    "D:/cricket07-browser-src/host"
    "D:/cricket07-browser-src/profiles"
    "D:/cricket07-browser-src/out_all"
)

add_compile_definitions(
    RC_PROFILE="cricket07.h"
)

# Recompiled static C translation units
file(GLOB RECOMP_SOURCES
    "D:/cricket07-browser-src/out_all/recomp_*.c"
)

add_library(cricket_recomp_objects OBJECT
    ${RECOMP_SOURCES}
    "D:/cricket07-browser-src/out_all/recomp_stubs.c"
)

# Shared Library for Android App (JNI / Activity)
add_library(cricket_native_harness SHARED
    src/main.c
    src/host_core.c
    "D:/cricket07-browser-src/host/kernel32.c"
    "D:/cricket07-browser-src/host/user32.c"
    "D:/cricket07-browser-src/host/misc.c"
    "D:/cricket07-browser-src/host/sched.c"
    "D:/cricket07-browser-src/host/settings.c"
    "D:/cricket07-browser-src/host/dinput.c"
    "D:/cricket07-browser-src/host/input.c"
    "D:/cricket07-browser-src/host/dsound.c"
    "D:/cricket07-browser-src/host/widescreen.c"
    "D:/cricket07-browser-src/runtime/runtime.c"
    "D:/cricket07-browser-src/runtime/interp.c"
    src/upstream_d3d8/com.c
    src/upstream_d3d8/d3d8.c
    src/upstream_d3d8/render_rec.c
    src/upstream_d3d8/render_wgpu.c
    src/upstream_d3d8/texconv.c
    src/upstream_d3d8/vshader.c
    src/upstream_d3d8/pshader.c
    $<TARGET_OBJECTS:cricket_recomp_objects>
)

target_link_directories(cricket_native_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/android-app/app/src/main/jniLibs/${ANDROID_ABI}"
)

target_link_libraries(cricket_native_harness PRIVATE
    wgpu_native
    android
    vulkan
    log
    m
    z
)

# Standalone Test Runner Executable
add_executable(cricket_authentic_runner
    src/authentic_boot_runner.c
    src/host_core.c
    "D:/cricket07-browser-src/host/kernel32.c"
    "D:/cricket07-browser-src/host/user32.c"
    "D:/cricket07-browser-src/host/misc.c"
    "D:/cricket07-browser-src/host/sched.c"
    "D:/cricket07-browser-src/host/settings.c"
    "D:/cricket07-browser-src/host/dinput.c"
    "D:/cricket07-browser-src/host/input.c"
    "D:/cricket07-browser-src/host/dsound.c"
    "D:/cricket07-browser-src/host/widescreen.c"
    "D:/cricket07-browser-src/runtime/runtime.c"
    "D:/cricket07-browser-src/runtime/interp.c"
    src/upstream_d3d8/com.c
    src/upstream_d3d8/d3d8.c
    src/upstream_d3d8/render_rec.c
    src/upstream_d3d8/render_wgpu.c
    src/upstream_d3d8/texconv.c
    src/upstream_d3d8/vshader.c
    src/upstream_d3d8/pshader.c
    $<TARGET_OBJECTS:cricket_recomp_objects>
)

target_link_directories(cricket_authentic_runner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/android-app/app/src/main/jniLibs/${ANDROID_ABI}"
)

target_link_libraries(cricket_authentic_runner PRIVATE
    wgpu_native
    android
    vulkan
    log
    m
    z
)
