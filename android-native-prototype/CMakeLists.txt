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
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Shared Library for Android App (JNI / Activity)
add_library(cricket_native_harness SHARED
    src/main.c
    src/cricket_memory.c
    src/test_physics.c
    src/test_graphics.c
)

target_link_libraries(cricket_native_harness
    android
    vulkan
    log
    m
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
