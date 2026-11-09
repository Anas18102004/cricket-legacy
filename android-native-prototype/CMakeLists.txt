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

add_library(cricket_native_harness SHARED
    src/main.c
    src/cricket_memory.c
    src/test_physics.c
)

# Link Android system libraries: logcat (__android_log_print) and math library (libm)
target_link_libraries(cricket_native_harness
    log
    m
)
