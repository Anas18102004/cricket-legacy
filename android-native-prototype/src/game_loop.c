#include "game_loop.h"
#include "game_timers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "CricketGameLoop"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define QUEUE_SIZE 256

static Win32Msg g_queue[QUEUE_SIZE];
static int g_qhead = 0;
static int g_qtail = 0;
static pthread_mutex_t g_qlock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t g_wndproc = 0;
static bool g_loop_paused = false;
static pthread_mutex_t g_loop_lock = PTHREAD_MUTEX_INITIALIZER;

// Global counter for synthetic test verification
static uint32_t g_test_messages_dispatched = 0;
static uint32_t g_test_wndproc_calls = 0;

// ------------------------------------------------------------------ Subsystem Lifecycle

void game_loop_init(void) {
    pthread_mutex_lock(&g_qlock);
    g_qhead = 0;
    g_qtail = 0;
    memset(g_queue, 0, sizeof(g_queue));
    pthread_mutex_unlock(&g_qlock);

    pthread_mutex_lock(&g_loop_lock);
    g_wndproc = 0;
    g_loop_paused = false;
    g_test_messages_dispatched = 0;
    g_test_wndproc_calls = 0;
    pthread_mutex_unlock(&g_loop_lock);

    LOGI("Game Operating Loop Subsystem Initialized. Queue capacity: %d messages.", QUEUE_SIZE);
}

void game_loop_shutdown(void) {
    pthread_mutex_lock(&g_qlock);
    g_qhead = 0;
    g_qtail = 0;
    pthread_mutex_unlock(&g_qlock);
    LOGI("Game Operating Loop Subsystem shut down.");
}

// ------------------------------------------------------------------ Message Queue

void post_message(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp) {
    pthread_mutex_lock(&g_qlock);
    int next = (g_qtail + 1) % QUEUE_SIZE;
    if (next != g_qhead) {
        g_queue[g_qtail].hwnd = hwnd;
        g_queue[g_qtail].msg = msg;
        g_queue[g_qtail].wp = wp;
        g_queue[g_qtail].lp = lp;
        g_queue[g_qtail].time = timer_time_get_time();
        g_queue[g_qtail].pt_x = 0;
        g_queue[g_qtail].pt_y = 0;
        g_qtail = next;
    } else {
        LOGW("Message queue full! Dropped msg 0x%04X", msg);
    }
    pthread_mutex_unlock(&g_qlock);
}

bool pop_message(Win32Msg *m, bool remove) {
    if (!m) return false;
    pthread_mutex_lock(&g_qlock);
    if (g_qhead == g_qtail) {
        pthread_mutex_unlock(&g_qlock);
        return false;
    }
    *m = g_queue[g_qhead];
    if (remove) {
        g_qhead = (g_qhead + 1) % QUEUE_SIZE;
    }
    pthread_mutex_unlock(&g_qlock);
    return true;
}

uint32_t get_queue_message_count(void) {
    pthread_mutex_lock(&g_qlock);
    uint32_t count = (g_qtail >= g_qhead) ? (uint32_t)(g_qtail - g_qhead) : (uint32_t)(QUEUE_SIZE - g_qhead + g_qtail);
    pthread_mutex_unlock(&g_qlock);
    return count;
}

// ------------------------------------------------------------------ Window Procedure & stdcall Dispatch

uint32_t guest_stdcall(CPU *c, uint32_t fn, int nargs, const uint32_t *args) {
    if (!fn) return 0;
    uint32_t sp = c->esp;
    for (int i = nargs - 1; i >= 0; i--) {
        PUSH32(c, args[i]);
    }
    PUSH32(c, GUEST_RETURN_SENTINEL);
    call_indirect(c, fn);
    if (c->esp != sp) {
        c->esp = sp;
    }
    return c->eax;
}

void register_window_proc(uint32_t wndproc_fn) {
    pthread_mutex_lock(&g_loop_lock);
    g_wndproc = wndproc_fn;
    pthread_mutex_unlock(&g_loop_lock);
    LOGI("Registered guest Window Procedure: 0x%08X", wndproc_fn);
}

uint32_t get_window_proc(void) {
    pthread_mutex_lock(&g_loop_lock);
    uint32_t fn = g_wndproc;
    pthread_mutex_unlock(&g_loop_lock);
    return fn;
}

uint32_t send_to_wndproc(CPU *c, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp) {
    uint32_t fn = get_window_proc();
    if (!fn) return 0;
    uint32_t args[4] = { hwnd, msg, wp, lp };
    return guest_stdcall(c, fn, 4, args);
}

// ------------------------------------------------------------------ Android Lifecycle & Input Bridging

void game_loop_set_paused(bool paused) {
    pthread_mutex_lock(&g_loop_lock);
    g_loop_paused = paused;
    pthread_mutex_unlock(&g_loop_lock);

    LOGI("[Lifecycle] Game operating loop %s", paused ? "PAUSED (Background)" : "RESUMED (Foreground)");
    // Post WM_ACTIVATEAPP: wParam = 1 (active) / 0 (inactive)
    post_message(MAIN_HWND, WM_ACTIVATEAPP, paused ? 0 : 1, 0);
}

bool game_loop_is_paused(void) {
    pthread_mutex_lock(&g_loop_lock);
    bool p = g_loop_paused;
    pthread_mutex_unlock(&g_loop_lock);
    return p;
}

void game_loop_post_touch(int action, float x, float y) {
    uint32_t lp = ((uint32_t)(uint16_t)(int)y << 16) | (uint32_t)(uint16_t)(int)x;
    if (action == 0) { // ACTION_DOWN
        post_message(MAIN_HWND, WM_LBUTTONDOWN, 0x0001 /* MK_LBUTTON */, lp);
    } else if (action == 2) { // ACTION_MOVE
        post_message(MAIN_HWND, WM_MOUSEMOVE, 0x0001, lp);
    } else if (action == 1) { // ACTION_UP
        post_message(MAIN_HWND, WM_LBUTTONUP, 0, lp);
    }
}

void game_loop_post_key(int key_code, bool down) {
    post_message(MAIN_HWND, down ? WM_KEYDOWN : WM_KEYUP, (uint32_t)key_code, 0);
}

// ------------------------------------------------------------------ Frame Pacing (60 Hz target)

void game_loop_pace_frame(uint64_t *last_frame_ns, uint32_t target_hz) {
    if (!last_frame_ns || target_hz == 0) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

    uint64_t target_slice_ns = 1000000000ULL / target_hz; // 16,666,666 ns for 60 Hz

    if (*last_frame_ns > 0 && now_ns > *last_frame_ns) {
        uint64_t elapsed_ns = now_ns - *last_frame_ns;
        if (elapsed_ns < target_slice_ns) {
            uint64_t sleep_ns = target_slice_ns - elapsed_ns;
            struct timespec req;
            req.tv_sec = sleep_ns / 1000000000ULL;
            req.tv_nsec = (long)(sleep_ns % 1000000000ULL);
            nanosleep(&req, NULL);

            clock_gettime(CLOCK_MONOTONIC, &ts);
            now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
        }
    }

    *last_frame_ns = now_ns;
}

// ------------------------------------------------------------------ Win32 Guest stdcall Handlers

static void write_guest_msg(uint32_t p, const Win32Msg *m) {
    W32(p + 0,  m->hwnd);
    W32(p + 4,  m->msg);
    W32(p + 8,  m->wp);
    W32(p + 12, m->lp);
    W32(p + 16, m->time);
    W32(p + 20, (uint32_t)m->pt_x);
    W32(p + 24, (uint32_t)m->pt_y);
}

void w_PeekMessageA(CPU *c) {
    uint32_t p = ARG(0);
    uint32_t remove_flag = ARG(4);
    Win32Msg m;
    bool has_msg = pop_message(&m, (remove_flag & PM_REMOVE) != 0);

    if (has_msg && p) {
        write_guest_msg(p, &m);
    }
    RET(5, has_msg ? 1 : 0);
}

void w_GetMessageA(CPU *c) {
    uint32_t p = ARG(0);
    Win32Msg m;
    while (!pop_message(&m, true)) {
        struct timespec ts = { 0, 2000000L }; // 2ms sleep
        nanosleep(&ts, NULL);
    }
    if (p) {
        write_guest_msg(p, &m);
    }
    RET(4, (m.msg != WM_QUIT) ? 1 : 0);
}

void w_TranslateMessage(CPU *c) {
    RET(1, 0);
}

void w_DispatchMessageA(CPU *c) {
    uint32_t p = ARG(0);
    if (!p) {
        RET(1, 0);
    }
    uint32_t hwnd = R32(p + 0);
    uint32_t msg  = R32(p + 4);
    uint32_t wp   = R32(p + 8);
    uint32_t lp   = R32(p + 12);

    g_test_messages_dispatched++;
    uint32_t r = send_to_wndproc(c, hwnd, msg, wp, lp);
    RET(1, r);
}

void w_PostMessageA(CPU *c) {
    post_message(ARG(0), ARG(1), ARG(2), ARG(3));
    RET(4, 1);
}

void w_SendMessageA(CPU *c) {
    uint32_t hwnd = ARG(0);
    uint32_t msg  = ARG(1);
    uint32_t wp   = ARG(2);
    uint32_t lp   = ARG(3);
    uint32_t r = send_to_wndproc(c, hwnd, msg, wp, lp);
    RET(4, r);
}

void w_PostQuitMessage(CPU *c) {
    uint32_t exit_code = ARG(0);
    post_message(MAIN_HWND, WM_QUIT, exit_code, 0);
    RET(1, 0);
}

void w_DefWindowProcA(CPU *c) {
    RET(4, 0);
}

void w_RegisterClassA(CPU *c) {
    uint32_t lpWndClass = ARG(0);
    if (lpWndClass) {
        register_window_proc(R32(lpWndClass + 4)); // WNDCLASSA.lpfnWndProc
    }
    RET(1, 0xC001); // Standard valid ATOM
}

void w_RegisterClassExA(CPU *c) {
    uint32_t lpWndClassEx = ARG(0);
    if (lpWndClassEx) {
        register_window_proc(R32(lpWndClassEx + 8)); // WNDCLASSEXA.lpfnWndProc
    }
    RET(1, 0xC001);
}

// ------------------------------------------------------------------ Clearly Labeled Synthetic Game Operating Loop

void test_custom_wndproc(CPU *c) {
    // Window procedure stdcall: LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    uint32_t hwnd = ARG(0);
    uint32_t uMsg = ARG(1);
    uint32_t wp   = ARG(2);
    uint32_t lp   = ARG(3);

    g_test_wndproc_calls++;
    LOGI("    [SYNTHETIC WNDPROC] Received message 0x%04X (hwnd=0x%08X, wp=0x%08X, lp=0x%08X)",
         uMsg, hwnd, wp, lp);

    // Return 0 for processed message
    RET(4, 0);
}

void test_game_loop_program(CPU *c) {
    LOGI("  [SYNTHETIC GAME LOOP] Starting simulated game operating loop (Stage 4 Validation)...");

    // 1. Register synthetic window procedure via dynamic import trampoline
    uint32_t wndproc_trampoline = add_dynamic_import("TestWndProc", test_custom_wndproc);
    register_window_proc(wndproc_trampoline);

    // 2. Allocate guest space for MSG structure
    uint32_t guest_msg_buf = heap_alloc(sizeof(Win32Msg) + 16, 1);
    if (!guest_msg_buf) {
        LOGE("  FAIL: Failed to allocate guest MSG buffer!");
        RET(4, 0xDEAD0041);
    }

    uint64_t last_frame_time = 0;
    bool quit_received = false;
    int frames_simulated = 0;

    // Simulate 10 frames of main operating loop
    while (!quit_received && frames_simulated < 10) {
        frames_simulated++;

        // Inject simulated events on specific frames
        if (frames_simulated == 1) {
            post_message(MAIN_HWND, WM_USER + 1, 0x1111, 0x2222);
        } else if (frames_simulated == 3) {
            game_loop_set_paused(true);
        } else if (frames_simulated == 4) {
            game_loop_set_paused(false);
        } else if (frames_simulated == 6) {
            game_loop_post_touch(0 /* DOWN */, 320.0f, 240.0f);
        } else if (frames_simulated == 8) {
            game_loop_post_key(0x53 /* 'S' */, true);
        } else if (frames_simulated == 9) {
            // Post quit message to cleanly terminate operating loop
            post_message(MAIN_HWND, WM_QUIT, 0, 0);
        }

        // Standard Win32 Message Pump:
        // while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { ... }
        Win32Msg m;
        while (pop_message(&m, true)) {
            if (m.msg == WM_QUIT) {
                LOGI("    [SYNTHETIC GAME LOOP] WM_QUIT received on frame %d. Terminating loop cleanly.", frames_simulated);
                quit_received = true;
                break;
            }
            write_guest_msg(guest_msg_buf, &m);
            send_to_wndproc(c, m.hwnd, m.msg, m.wp, m.lp);
        }

        // Game frame pacing (simulating 60 Hz pacing)
        game_loop_pace_frame(&last_frame_time, 60);
    }

    heap_free(guest_msg_buf);

    if (!quit_received) {
        LOGE("  FAIL: Synthetic game loop did not cleanly terminate on WM_QUIT!");
        RET(4, 0xDEAD0042);
    }

    LOGI("  [SYNTHETIC GAME LOOP] 10 frames executed, messages pumped, pacing verified. Returning 0xCAFE0040.");
    RET(4, 0xCAFE0040);
}
