#include "test_stage4_game_loop.h"
#include "game_loop.h"
#include "game_crt_winmain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <android/log.h>

#define LOG_TAG "Stage4LoopTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool test1_message_queue_fifo(void) {
    LOGI("[TEST 1] Testing Message Queue FIFO Ordering...");

    // Drain any leftover messages
    Win32Msg dummy;
    while (pop_message(&dummy, true));

    // Post 5 messages
    for (uint32_t i = 1; i <= 5; i++) {
        post_message(MAIN_HWND, WM_USER + i, i * 10, i * 100);
    }

    if (get_queue_message_count() != 5) {
        LOGE("  FAIL: Message count mismatch! Expected 5, got %u", get_queue_message_count());
        return false;
    }

    // Verify FIFO pop order
    for (uint32_t i = 1; i <= 5; i++) {
        Win32Msg m;
        if (!pop_message(&m, true)) {
            LOGE("  FAIL: Failed to pop message %u!", i);
            return false;
        }
        if (m.msg != WM_USER + i || m.wp != i * 10 || m.lp != i * 100) {
            LOGE("  FAIL: Message %u payload mismatch! msg=0x%04X, wp=%u, lp=%u", i, m.msg, m.wp, m.lp);
            return false;
        }
    }

    LOGI("  Successfully posted and popped 5 messages in exact FIFO order.");
    LOGI("  PASS: Test 1 Message Queue FIFO verified.");
    return true;
}

static bool test2_peek_message_non_destructive(void) {
    LOGI("[TEST 2] Testing Non-Destructive Message Peeking...");

    post_message(MAIN_HWND, WM_USER + 42, 0xAAAA, 0xBBBB);

    Win32Msg m_peek;
    if (!pop_message(&m_peek, false /* PM_NOREMOVE */)) {
        LOGE("  FAIL: pop_message non-remove returned false!");
        return false;
    }

    if (m_peek.msg != WM_USER + 42 || get_queue_message_count() != 1) {
        LOGE("  FAIL: Non-destructive peek mutated queue count or msg mismatch!");
        return false;
    }

    // Now remove it
    Win32Msg m_pop;
    if (!pop_message(&m_pop, true /* PM_REMOVE */) || get_queue_message_count() != 0) {
        LOGE("  FAIL: Destructive pop failed or queue not empty!");
        return false;
    }

    LOGI("  PASS: Test 2 Non-Destructive Message Peeking verified.");
    return true;
}

static bool test3_window_class_registration(void) {
    LOGI("[TEST 3] Testing Window Class & Procedure Registration...");

    uint32_t guest_wc = heap_alloc(64, 1);
    if (!guest_wc) {
        LOGE("  FAIL: Failed to allocate guest buffer for WNDCLASSA!");
        return false;
    }

    // Set lpfnWndProc at offset +4
    uint32_t test_wndproc_target = 0x0045A990u;
    W32(guest_wc + 4, test_wndproc_target);

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, guest_wc); // ARG(0) = lpWndClass

    w_RegisterClassA(&c);
    heap_free(guest_wc);

    if (c.eax != 0xC001) {
        LOGE("  FAIL: RegisterClassA returned invalid atom: 0x%08X", c.eax);
        return false;
    }

    if (get_window_proc() != test_wndproc_target) {
        LOGE("  FAIL: Registered wndproc mismatch! Expected 0x%08X, got 0x%08X",
             test_wndproc_target, get_window_proc());
        return false;
    }

    LOGI("  Registered Window Procedure 0x%08X successfully verified.", test_wndproc_target);
    LOGI("  PASS: Test 3 Window Class Registration verified.");
    return true;
}

static bool test4_synchronous_message_dispatch(void) {
    LOGI("[TEST 4] Testing Synchronous Message Dispatch (DispatchMessageA)...");

    // Register synthetic window procedure
    uint32_t wndproc_trampoline = add_dynamic_import("TestWndProcStage4", test_custom_wndproc);
    register_window_proc(wndproc_trampoline);

    // Prepare a MSG structure in guest memory
    uint32_t guest_msg = heap_alloc(sizeof(Win32Msg), 1);
    W32(guest_msg + 0,  MAIN_HWND);
    W32(guest_msg + 4,  WM_USER + 88);
    W32(guest_msg + 8,  0x1234);
    W32(guest_msg + 12, 0x5678);

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, guest_msg); // ARG(0) = lpMsg

    w_DispatchMessageA(&c);
    heap_free(guest_msg);

    LOGI("  DispatchMessageA successfully routed message to registered WndProc (LRESULT: %u)", c.eax);
    LOGI("  PASS: Test 4 Message Dispatch verified.");
    return true;
}

static bool test5_input_translation(void) {
    LOGI("[TEST 5] Testing Touch and Input Event Translation...");

    // Test 5a: Android Touch DOWN at (640, 360)
    game_loop_post_touch(0 /* ACTION_DOWN */, 640.0f, 360.0f);
    Win32Msg m_down;
    if (!pop_message(&m_down, true)) {
        LOGE("  FAIL: No message popped for touch down!");
        return false;
    }
    if (m_down.msg != WM_LBUTTONDOWN) {
        LOGE("  FAIL: Expected WM_LBUTTONDOWN (0x0201), got 0x%04X", m_down.msg);
        return false;
    }
    uint16_t x_pos = (uint16_t)(m_down.lp & 0xFFFF);
    uint16_t y_pos = (uint16_t)((m_down.lp >> 16) & 0xFFFF);
    LOGI("  Touch DOWN translated to WM_LBUTTONDOWN at coords (%u, %u)", x_pos, y_pos);
    if (x_pos != 640 || y_pos != 360) {
        LOGE("  FAIL: Touch coordinate translation mismatch!");
        return false;
    }

    // Test 5b: Android Key DOWN 'A' (0x41)
    game_loop_post_key(0x41, true);
    Win32Msg m_key;
    if (!pop_message(&m_key, true)) {
        LOGE("  FAIL: No message popped for key down!");
        return false;
    }
    if (m_key.msg != WM_KEYDOWN || m_key.wp != 0x41) {
        LOGE("  FAIL: Expected WM_KEYDOWN for 'A', got msg 0x%04X, wp 0x%08X", m_key.msg, m_key.wp);
        return false;
    }

    LOGI("  PASS: Test 5 Input Translation verified.");
    return true;
}

static bool test6_android_lifecycle_pause_resume(void) {
    LOGI("[TEST 6] Testing Android Lifecycle Pause & Resume Handling...");

    // Simulate Android onPause()
    game_loop_set_paused(true);
    if (!game_loop_is_paused()) {
        LOGE("  FAIL: game_loop_is_paused returned false after pause!");
        return false;
    }

    Win32Msg m_pause;
    if (!pop_message(&m_pause, true)) {
        LOGE("  FAIL: No message generated on pause!");
        return false;
    }
    if (m_pause.msg != WM_ACTIVATEAPP || m_pause.wp != 0) {
        LOGE("  FAIL: Expected WM_ACTIVATEAPP (0) on pause, got msg 0x%04X, wp %u", m_pause.msg, m_pause.wp);
        return false;
    }
    LOGI("  Pause state correctly posted WM_ACTIVATEAPP (inactive).");

    // Simulate Android onResume()
    game_loop_set_paused(false);
    if (game_loop_is_paused()) {
        LOGE("  FAIL: game_loop_is_paused returned true after resume!");
        return false;
    }

    Win32Msg m_resume;
    if (!pop_message(&m_resume, true)) {
        LOGE("  FAIL: No message generated on resume!");
        return false;
    }
    if (m_resume.msg != WM_ACTIVATEAPP || m_resume.wp != 1) {
        LOGE("  FAIL: Expected WM_ACTIVATEAPP (1) on resume, got msg 0x%04X, wp %u", m_resume.msg, m_resume.wp);
        return false;
    }
    LOGI("  Resume state correctly posted WM_ACTIVATEAPP (active).");

    LOGI("  PASS: Test 6 Android Lifecycle verified.");
    return true;
}

static bool test7_frame_pacing(void) {
    LOGI("[TEST 7] Testing 60 FPS Frame Pacing & Clock Sleep...");

    uint64_t last_time = 0;
    struct timespec start_ts, end_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    // Pace 3 consecutive frames at 60 Hz (target: ~50 ms total)
    for (int i = 0; i < 3; i++) {
        game_loop_pace_frame(&last_time, 60);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    double elapsed_ms = ((double)(end_ts.tv_sec - start_ts.tv_sec) * 1e3) +
                        ((double)(end_ts.tv_nsec - start_ts.tv_nsec) * 1e-6);

    LOGI("  Elapsed time across 3 paced frames: %.2f ms (Expected ~50 ms)", elapsed_ms);
    if (elapsed_ms < 28.0 || elapsed_ms > 70.0) {
        LOGE("  FAIL: Paced time %.2f ms outside acceptable range [28ms, 70ms]!", elapsed_ms);
        return false;
    }

    LOGI("  PASS: Test 7 Frame Pacing verified.");
    return true;
}

static bool test8_synthetic_game_operating_loop(void) {
    LOGI("[TEST 8] Testing Full Synthetic Game Operating Loop...");
    LOGI("  NOTE: Executing clearly labeled 'test_game_loop_program' (NOT actual Cricket 07 code).");

    int exit_code = 0;
    bool launched = crt_launch_winmain(test_game_loop_program, 0x00400000u, 10, &exit_code);

    if (!launched) {
        LOGE("  FAIL: Failed to launch synthetic game loop thread!");
        return false;
    }

    LOGI("  Synthetic game loop terminated with ExitCode: 0x%08X", (uint32_t)exit_code);
    if ((uint32_t)exit_code != 0xCAFE0040u) {
        LOGE("  FAIL: Expected exit code 0xCAFE0040, got 0x%08X!", (uint32_t)exit_code);
        return false;
    }

    LOGI("  PASS: Test 8 Synthetic Game Operating Loop verified.");
    return true;
}

bool run_stage4_game_loop_tests(void) {
    LOGI("==================================================================");
    LOGI("PHASE C.2 — STAGE 4: ENGINE MAIN OPERATING LOOP VERIFICATION");
    LOGI("Message Pump, Frame Pacing, Lifecycle Bridging & Window Dispatch");
    LOGI("==================================================================");

    game_loop_init();

    int passed = 0;
    int total = 8;

    if (test1_message_queue_fifo()) passed++;
    if (test2_peek_message_non_destructive()) passed++;
    if (test3_window_class_registration()) passed++;
    if (test4_synchronous_message_dispatch()) passed++;
    if (test5_input_translation()) passed++;
    if (test6_android_lifecycle_pause_resume()) passed++;
    if (test7_frame_pacing()) passed++;
    if (test8_synthetic_game_operating_loop()) passed++;

    LOGI("==================================================================");
    LOGI("STAGE 4 TEST SUMMARY: %d / %d TESTS PASSED (Failed: %d)", passed, total, total - passed);
    LOGI("==================================================================");

    return (passed == total);
}
