cmake_minimum_required(VERSION 3.22.1)
project(cricket_native_harness C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enforce no-strict-aliasing for recompiled x86 pointer arithmetic
add_compile_options(
    -O2
    -Wall
    -fno-strict-aliasing
    -fvisibility=default
    -Wno-unused-function
    -Wno-unused-variable
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/upstream_d3d8
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Shared Library for Android App (JNI / Activity)
add_library(cricket_native_harness SHARED
    src/main.c
    src/cricket_memory.c
    src/test_physics.c
    src/test_graphics.c
    src/test_wgpu_render.c
    src/upstream_d3d8/com.c
    src/upstream_d3d8/d3d8.c
    src/upstream_d3d8/upstream_core.c
    src/upstream_d3d8/texconv.c
    src/upstream_d3d8/vshader.c
    src/upstream_d3d8/pshader.c
    src/upstream_d3d8/render_rec.c
    src/upstream_d3d8/render_wgpu.c
    src/upstream_d3d8/d3d8_android_adapter.c
    src/test_d3d8_scene.c
    src/game_static_data.c
    src/test_stage1_memory.c
)

target_link_directories(cricket_native_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/android-app/app/src/main/jniLibs/${ANDROID_ABI}"
)

target_link_libraries(cricket_native_harness
    wgpu_native
    android
    vulkan
    log
    m
    z
)

# Standalone Test Runner Executable (direct CLI / adb execution)
add_executable(cricket_test_runner
    src/test_memory_runner.c
    src/cricket_memory.c
    src/test_physics.c
)

target_link_libraries(cricket_test_runner
    android
    log
    m
)
