#include <jni.h>
#include <android/log.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "cricket_memory.h"
#include "test_physics.h"

#define LOG_TAG "CricketNativeHarness"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static CricketVirtualArena g_arena;
static bool g_is_running = false;
static uint64_t g_frame_count = 0;
static struct timespec g_last_frame_time;

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// JNI Entry point: Initialize the native mobile harness
JNIEXPORT jboolean JNICALL
Java_org_libsdl_app_SDLActivity_nativeCricketHarnessInit(JNIEnv* env, jclass clazz) {
    LOGI("==================================================");
    LOGI("CRICKET LEGACY — NATIVE ANDROID HARNESS STARTUP");
    LOGI("Target ABI: ARM64-v8a | NDK Clang 18 Toolchain");
    LOGI("==================================================");

    // 1. Initialize 4GB virtual arena for 32-bit x86 pointer translation
    if (!cricket_memory_init(&g_arena, 0)) {
        LOGE("CRITICAL: Virtual address space allocation failed!");
        return JNI_FALSE;
    }

    // 2. Test 32-bit pointer translation and unaligned access
    uint32_t test_x86_offset = 0x0045A120; // Simulated function/struct offset
    cricket_write_u32(&g_arena, test_x86_offset, 0x12345678);
    uint32_t read_back = cricket_read_u32(&g_arena, test_x86_offset);
    if (read_back == 0x12345678) {
        LOGI("Memory Arena 32-bit translation check PASSED (Offset 0x%08X = 0x%08X)", 
             test_x86_offset, read_back);
    } else {
        LOGE("Memory Arena translation MISMATCH! Expected 0x12345678, got 0x%08X", read_back);
        return JNI_FALSE;
    }

    // 3. Execute ARM64 ballistic physics verification test
    if (!cricket_physics_run_validation_test()) {
        LOGE("Physics validation test failed!");
        return JNI_FALSE;
    }

    // 4. Initial telemetry
    size_t rss_kb = cricket_get_resident_memory_kb();
    LOGI("Initial Native Resident RAM: %zu KB (%.2f MB)", rss_kb, (float)rss_kb / 1024.0f);

    clock_gettime(CLOCK_MONOTONIC_RAW, &g_last_frame_time);
    g_is_running = true;
    g_frame_count = 0;

    LOGI("Cricket Native Harness successfully initialized.");
    return JNI_TRUE;
}

// JNI Entry point: Handle touchscreen input gestures
JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeCricketOnTouch(JNIEnv* env, jclass clazz, 
                                                    jint action, jfloat x, jfloat y, jint pointer_id) {
    // Action: 0 = DOWN, 1 = UP, 2 = MOVE
    uint64_t now_ns = get_time_ns();
    LOGI("Touch Event: Action=%d, Pos=(%.1f, %.1f), Finger=%d, Time=%llu ns", 
         action, x, y, pointer_id, (unsigned long long)now_ns);
}

// JNI Entry point: Update simulation and measure frame pacing
JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeCricketRenderFrame(JNIEnv* env, jclass clazz) {
    if (!g_is_running) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);

    uint64_t elapsed_ns = (uint64_t)(now.tv_sec - g_last_frame_time.tv_sec) * 1000000000ULL +
                          (uint64_t)(now.tv_nsec - g_last_frame_time.tv_nsec);
    g_last_frame_time = now;
    g_frame_count++;

    // Telemetry log every 120 frames (~2 seconds at 60 FPS)
    if (g_frame_count % 120 == 0) {
        float frame_ms = (float)elapsed_ns / 1000000.0f;
        float fps = (frame_ms > 0.001f) ? (1000.0f / frame_ms) : 0.0f;
        size_t rss_kb = cricket_get_resident_memory_kb();
        LOGI("Telemetry: Frame #%llu | FrameTime: %.2f ms (%.1f FPS) | RAM: %zu KB",
             (unsigned long long)g_frame_count, frame_ms, fps, rss_kb);
    }
}

// JNI Entry point: Shutdown and release resources
JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeCricketHarnessShutdown(JNIEnv* env, jclass clazz) {
    LOGI("Shutting down Cricket Native Harness...");
    g_is_running = false;
    cricket_memory_cleanup(&g_arena);
    LOGI("Harness shutdown complete.");
}
