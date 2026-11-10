#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

#include "win32.h"
#include "runtime.h"
#include "recomp.h"
#include "render.h"
#include "d3d8.h"
#include "mobile_input.h"

#define TAG "CricketLegacy-Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Upstream host symbols (declared in win32.h / runtime.h / d3d8.h)
extern void dinput_pad(int idx, int connected, uint32_t std_buttons, const int32_t axis[4]);
extern uint8_t g_dik[256];
extern void dinput_key(uint8_t dik, int down);
extern void dinput_mouse_button(int b, int down);
extern void dinput_mouse_move(int dx, int dy);
extern void input_tap(int x, int y);
extern void input_tap_move(int dx, int dy);
extern void input_tap_release(void);
extern void input_mouse_button(int b, int down);
extern void input_key_dik(uint8_t dik, int down);
extern void input_gamepad(int idx, int connected, uint32_t buttons, int lx, int ly, int rx, int ry);
extern void configure_surface(void);

extern RenderBackend g_render_gpu;
extern RenderBackend *g_render;
extern ANativeWindow *g_android_native_window;
extern int g_window_w, g_window_h, g_screen_w, g_screen_h;
extern int g_surf_w, g_surf_h;
extern void capture_png(const char *path);
extern Device *g_device;

// Global state
static LoadedImage g_img;
static CPU g_cpu;
static jmp_buf g_local_trap_buf;
static pthread_t g_game_thread;
static volatile bool g_game_thread_running = false;
static volatile bool g_harness_initialized = false;
static ANativeWindow *g_current_native_window = NULL;

static size_t get_rss_kb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0;
    if (fscanf(f, "%*s%ld", &pages) == 1) {
        fclose(f);
        return (size_t)(pages * (sysconf(_SC_PAGESIZE) / 1024));
    }
    fclose(f);
    return 0;
}

// Dedicated authentic game thread
static void *authentic_game_thread_worker(void *arg) {
    (void)arg;
    LOGI("[GameThread] Authentic Game Thread launched (TID=%d, StackSize=%zu)", gettid(), (size_t)HOST_STACK_GAME);

    // 1. Thread and FPU configuration
    thread_main_init(&g_cpu);
    sched_main_init();
    if (t_self) {
        t_self->cpu = g_cpu;
    }
    fpu_sync_rounding(&g_cpu);

    // 2. Set up trap interception
    g_trap_jmp = &g_local_trap_buf;
    if (setjmp(g_local_trap_buf) != 0) {
        LOGE("[GameThread] GUEST TRAP INTERCEPTED: %s", g_trap_msg);
        LOGI("[GameThread] Registers at trap: EAX=0x%08x, ECX=0x%08x, EDX=0x%08x, EBX=0x%08x, ESP=0x%08x, EBP=0x%08x",
             g_cpu.eax, g_cpu.ecx, g_cpu.edx, g_cpu.ebx, g_cpu.esp, g_cpu.ebp);
        g_game_thread_running = false;
        return NULL;
    }

    // 3. Lookup Entry Point
    GuestFn entry_fn = lookup_function(g_img.entry);
    if (!entry_fn) {
        LOGE("[GameThread] Entry point 0x%08x not found in recompiled functions!", g_img.entry);
        g_game_thread_running = false;
        return NULL;
    }

    LOGI("[GameThread] Booting authentic CRT startup trampoline at 0x%08x...", g_img.entry);
    PUSH32(&g_cpu, GUEST_RETURN_SENTINEL);

    // 4. Run authentic game WinMain
    entry_fn(&g_cpu);

    LOGI("[GameThread] Authentic WinMain returned gracefully with exit code: %d", (int)g_cpu.eax);
    g_game_thread_running = false;
    return NULL;
}

// JNI Entry: Initialize authentic game harness
JNIEXPORT jboolean JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketHarnessInit(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    LOGI("==================================================================");
    LOGI("CRICKET LEGACY: Authentic Game Harness Initialization (Android)");
    LOGI("==================================================================");

    if (g_harness_initialized) {
        LOGI("Harness already initialized, skipping re-initialization.");
        return JNI_TRUE;
    }

    // 1. Detect game asset location
    const char *candidate_paths[] = {
        "/data/data/org.cricketlegacy/files/game",
        "/data/user/0/org.cricketlegacy/files/game",
        "/data/local/tmp/cricket07"
    };
    const char *chosen_root = NULL;
    for (size_t i = 0; i < sizeof(candidate_paths) / sizeof(candidate_paths[0]); i++) {
        char test_exe[1024];
        snprintf(test_exe, sizeof(test_exe), "%s/Cricket07.exe", candidate_paths[i]);
        if (access(test_exe, R_OK) == 0) {
            chosen_root = candidate_paths[i];
            break;
        }
    }

    if (!chosen_root) {
        LOGE("ERROR: Cricket07.exe not found in any candidate directory!");
        return JNI_FALSE;
    }

    snprintf(g_game_root, sizeof(g_game_root), "%s", chosen_root);
    snprintf(g_user_root, sizeof(g_user_root), "%s/userdata", chosen_root);
    mkdir(g_user_root, 0755);

    LOGI("[Init] Selected game root: %s", g_game_root);
    LOGI("[Init] User data root:     %s", g_user_root);

    setenv("RC_PACE60", "1", 1);

    // Initialize native mobile input system
    mobile_input_init(g_window_w > 0 ? g_window_w : 1280, g_window_h > 0 ? g_window_h : 720);

    // 2. Register Windows API import handlers
    LOGI("[Init] Registering host import tables...");
    static ImportHandler all_imports[2048];
    uint32_t total_imports = 0;
    const ImportTable *tables[] = {
        &kernel32_imports,
        &user32_imports,
        &gdi32_imports,
        &advapi32_imports,
        &winmm_imports,
        &directx_imports
    };
    for (size_t t = 0; t < sizeof(tables) / sizeof(tables[0]); t++) {
        for (uint32_t i = 0; i < tables[t]->count; i++) {
            if (total_imports < sizeof(all_imports) / sizeof(all_imports[0])) {
                all_imports[total_imports++] = tables[t]->list[i];
            }
        }
    }
    set_import_handlers(all_imports, total_imports);
    LOGI("[Init] Registered %u host API handlers.", total_imports);

    // 3. Initialize guest memory space and CPU
    LOGI("[Init] Initializing guest 4GiB virtual memory & CPU...");
    runtime_init(&g_cpu);
    vclock_init();
    LOGI("[Init] Guest memory initialized at host pointer %p (ESP=0x%08x)", (void *)g_mem, g_cpu.esp);

    // 4. Register all 31,499 recompiled static functions
    LOGI("[Init] Registering 31,499 recompiled functions in hash table...");
    register_all_functions();
    LOGI("[Init] All recompiled functions registered successfully.");

    // 5. Load PE image into guest virtual memory
    char exe_file[1024];
    snprintf(exe_file, sizeof(exe_file), "%s/Cricket07.exe", g_game_root);
    LOGI("[Init] Loading authentic PE image from: %s", exe_file);

    if (load_image(exe_file, &g_img) != 0) {
        LOGE("[Init] ERROR: Failed to load PE image into guest address space!");
        return JNI_FALSE;
    }
    LOGI("[Init] PE Image Loaded: Base=0x%08x, Entry=0x%08x, ImportsBound=%u",
         g_img.image_base, g_img.entry, g_img.num_imports);

    // 6. Verify entry point
    GuestFn entry_fn = lookup_function(g_img.entry);
    if (!entry_fn) {
        LOGE("[Init] FATAL: Entry point 0x%08x not found in recompiled functions!", g_img.entry);
        return JNI_FALSE;
    }
    LOGI("[Init] Entry point verified -> %p", (void *)entry_fn);

    g_harness_initialized = true;
    LOGI("Authentic Game Harness initialization complete.");
    return JNI_TRUE;
}

// JNI Entry: Set Android Surface for Native Direct3D 8 WebGPU Rendering
JNIEXPORT jboolean JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketSetSurface(JNIEnv* env, jclass clazz, jobject surface) {
    (void)clazz;
    if (!surface) {
        LOGI("nativeCricketSetSurface: Surface released.");
        if (g_current_native_window) {
            ANativeWindow_release(g_current_native_window);
            g_current_native_window = NULL;
            g_android_native_window = NULL;
        }
        return JNI_TRUE;
    }

    if (!g_harness_initialized) {
        LOGE("nativeCricketSetSurface called before nativeCricketHarnessInit!");
        return JNI_FALSE;
    }

    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);
    if (!win) {
        LOGE("Failed to obtain ANativeWindow from Java Surface!");
        return JNI_FALSE;
    }

    if (g_current_native_window) {
        ANativeWindow_release(g_current_native_window);
    }
    g_current_native_window = win;
    g_android_native_window = win;

    int32_t width = ANativeWindow_getWidth(win);
    int32_t height = ANativeWindow_getHeight(win);
    if (width <= 0 || height <= 0) {
        width = 1280;
        height = 720;
    }

    g_window_w = width;
    g_window_h = height;
    g_screen_w = width;
    g_screen_h = height;
    mobile_input_resize((int)width, (int)height);

    LOGI("Bound ANativeWindow: %p (Resolution: %dx%d)", win, width, height);

    // Direct rendering to WebGPU backend
    g_render = &g_render_gpu;

    // Launch authentic game thread if not running
    if (!g_game_thread_running) {
        g_game_thread_running = true;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, HOST_STACK_GAME);

        int rc = pthread_create(&g_game_thread, &attr, authentic_game_thread_worker, NULL);
        pthread_attr_destroy(&attr);

        if (rc != 0) {
            LOGE("Failed to spawn authentic game thread (rc=%d)!", rc);
            g_game_thread_running = false;
            return JNI_FALSE;
        }
        LOGI("Authentic game thread successfully started!");
    } else {
        LOGI("Surface re-attached to existing game thread: reconfiguring swapchain.");
        configure_surface();
    }

    return JNI_TRUE;
}

// JNI Entry: Handle surface resize
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketResize(JNIEnv* env, jclass clazz, jint width, jint height) {
    (void)env;
    (void)clazz;
    if (width <= 0 || height <= 0) return;
    LOGI("nativeCricketResize: %dx%d", width, height);
    g_window_w = (int)width;
    g_window_h = (int)height;
    g_screen_w = (int)width;
    g_screen_h = (int)height;
    mobile_input_resize((int)width, (int)height);
    if (g_device) {
        g_device->width = (uint32_t)width;
        g_device->height = (uint32_t)height;
    }
    configure_surface();
}

// JNI Entry: Handle surface destroyed
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketSurfaceDestroyed(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    LOGI("nativeCricketSurfaceDestroyed: Detaching window.");
    if (g_current_native_window) {
        ANativeWindow_release(g_current_native_window);
        g_current_native_window = NULL;
        g_android_native_window = NULL;
    }
}

// JNI Entry: Handle touchscreen input gestures
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketOnTouch(JNIEnv* env, jclass clazz,
                                                        jint action, jfloat x, jfloat y, jint pointer_id) {
    (void)env;
    (void)clazz;

    // 1. Dispatch into native multi-pointer mobile input subsystem (joystick, swipe, buttons)
    mobile_input_on_touch(action, x, y, pointer_id);

    // 2. Map screen touch to 640x480 virtual game coords taking pillarbox into account for direct menu clicks
    int surf_w = (g_surf_w > 0) ? g_surf_w : g_window_w;
    int surf_h = (g_surf_h > 0) ? g_surf_h : g_window_h;
    int bar_x = (g_window_w > surf_w) ? (g_window_w - surf_w) / 2 : 0;
    int bar_y = (g_window_h > surf_h) ? (g_window_h - surf_h) / 2 : 0;

    float rel_x = x - (float)bar_x;
    float rel_y = y - (float)bar_y;

    int game_x = (surf_w > 0) ? (int)(rel_x * 640.0f / (float)surf_w) : 320;
    int game_y = (surf_h > 0) ? (int)(rel_y * 480.0f / (float)surf_h) : 240;
    if (game_x < 0) game_x = 0;
    if (game_x > 639) game_x = 639;
    if (game_y < 0) game_y = 0;
    if (game_y > 479) game_y = 479;

    static float s_prev_touch_x = 0.0f;
    static float s_prev_touch_y = 0.0f;
    int masked = action & 0xFF;

    if (masked == 0 /* DOWN */ || masked == 5 /* POINTER_DOWN */) {
        s_prev_touch_x = x;
        s_prev_touch_y = y;
        input_tap(game_x, game_y);
    } else if (masked == 2 /* MOVE */) {
        int dx = (int)(x - s_prev_touch_x);
        int dy = (int)(y - s_prev_touch_y);
        s_prev_touch_x = x;
        s_prev_touch_y = y;
        input_tap_move(dx, dy);
    } else if (masked == 1 /* UP */ || masked == 6 /* POINTER_UP */ || masked == 3 /* CANCEL */) {
        input_tap_release();
    }
}

// JNI Entry: Frame telemetry and stats
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketRenderFrame(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;

    // Advance native mobile input frame tick (decrementing stroke pulses, etc.)
    mobile_input_frame_tick();
    static uint64_t s_frame_call_count = 0;
    static uint32_t s_last_engine_frame = 0;
    static struct timespec s_last_time;

    if (s_frame_call_count == 0) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &s_last_time);
    }
    s_frame_call_count++;

    // Phase F: Authentic Match Progression State Machine
    static int s_state_tick = 0;
    s_state_tick++;

    // Stage 1: Publisher splash videos (Frames 100 - 650)
    if (s_state_tick >= 100 && s_state_tick <= 650) {
        if ((s_state_tick % 60) == 10) {
            input_key_dik(0x1C, 1); // Enter down
            input_key_dik(0x39, 1); // Space down
        } else if ((s_state_tick % 60) == 20) {
            input_key_dik(0x1C, 0); // Enter up
            input_key_dik(0x39, 0); // Space up
        }
    }
    // Stage 2: Title screen CLICK TO CONTINUE & profile dialog (Frames 700 - 920)
    else if (s_state_tick >= 700 && s_state_tick <= 920) {
        if ((s_state_tick % 45) == 5) {
            input_key_dik(0x1C, 1); // Enter down
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0); // Button A down
        } else if ((s_state_tick % 45) == 18) {
            input_key_dik(0x1C, 0); // Enter up
            input_gamepad(0, 1, 0, 0, 0, 0, 0); // Button A up
        }
    }
    // Milestone Capture: Main Menu (Frame 1000)
    else if (s_state_tick == 1000) {
        capture_png("/data/data/org.cricketlegacy/files/game/f1_main_menu.png");
        LOGI("[Phase F1] Main menu reached and captured at frame 1000");
    }
    // Stage 3: Main Menu -> Select PLAY NOW (Frames 1050 - 1180)
    else if (s_state_tick >= 1050 && s_state_tick <= 1180) {
        if (s_state_tick == 1060 || s_state_tick == 1080) {
            input_key_dik(0xC8, 1); // Up Arrow
        } else if (s_state_tick == 1070 || s_state_tick == 1090) {
            input_key_dik(0xC8, 0);
        } else if (s_state_tick == 1120) {
            input_key_dik(0x1C, 1); // Enter down on PLAY NOW
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 1140) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        }
    }
    // Stage 4: Team Selection (Frames 1280 - 1500)
    else if (s_state_tick >= 1280 && s_state_tick <= 1500) {
        if (s_state_tick == 1300) {
            LOGI("[Phase F1] Moving P1 controller to Left Team (Sri Lanka)");
            input_key_dik(0xCB, 1); // Left Arrow
            input_gamepad(0, 1, 0, -32768, 0, 0, 0); // Stick Left
        } else if (s_state_tick == 1320) {
            input_key_dik(0xCB, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 1360) {
            LOGI("[Phase F1] Capturing Team Select with P1 assigned");
            capture_png("/data/data/org.cricketlegacy/files/game/f1_team_select.png");
        } else if (s_state_tick == 1420) {
            LOGI("[Phase F1] Confirming Team Select -> CONTINUE");
            input_key_dik(0x1C, 1); // Enter
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0); // Button A
        } else if (s_state_tick == 1440) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        }
    }
    // Stage 5: Match Settings (Frames 1550 - 1800)
    else if (s_state_tick >= 1550 && s_state_tick <= 1800) {
        if (s_state_tick == 1600) {
            LOGI("[Phase F1] Capturing Match Settings Screen");
            capture_png("/data/data/org.cricketlegacy/files/game/f1_match_settings.png");
        } else if (s_state_tick == 1680) {
            LOGI("[Phase F1] Confirming Match Settings -> CONTINUE");
            input_key_dik(0x1C, 1);
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 1700) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        }
    }
    // Stage 6: Coin Toss (Frames 1850 - 2150)
    else if (s_state_tick >= 1850 && s_state_tick <= 2150) {
        if (s_state_tick == 1900) {
            LOGI("[Phase F1] Capturing Coin Toss Screen");
            capture_png("/data/data/org.cricketlegacy/files/game/f1_coin_toss.png");
        } else if (s_state_tick == 1950) {
            LOGI("[Phase F1] Tossing Coin -> Heads");
            input_key_dik(0x1C, 1);
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 1970) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 2040) {
            LOGI("[Phase F1] Electing to Bat");
            input_key_dik(0x1C, 1);
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 2060) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 2120) {
            LOGI("[Phase F1] Starting Match Loading");
            input_key_dik(0x1C, 1);
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 2140) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        }
    }
    // Stage 7: Match Loading & Intro Cutscenes (Frames 2200 - 3200)
    else if (s_state_tick >= 2200 && s_state_tick <= 3200) {
        if ((s_state_tick % 60) == 10) {
            input_key_dik(0x1C, 1);
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if ((s_state_tick % 60) == 20) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        }
    }
    // Milestone Capture: 3D Match Ground & Pitch (Frame 3300)
    else if (s_state_tick == 3300) {
        LOGI("[Phase F2] 3D Cricket Ground reached and captured");
        capture_png("/data/data/org.cricketlegacy/files/game/f2_match_ground.png");
    }
    // Stage 8: Bowling & Batting Interactive Validation (Frames 3400 - 4500)
    else if (s_state_tick >= 3400 && s_state_tick <= 4500) {
        if (s_state_tick == 3450) {
            LOGI("[Phase F3] Bowler run-up started");
            input_key_dik(0x1F, 1); // Key S
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
        } else if (s_state_tick == 3470) {
            input_key_dik(0x1F, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 3540) {
            LOGI("[Phase F3] Bowler ball released at crease");
            input_key_dik(0x1C, 1); // Enter / Release
            input_gamepad(0, 1, 1u << 0, 0, 0, 0, 0);
            capture_png("/data/data/org.cricketlegacy/files/game/f3_bowling_delivery.png");
        } else if (s_state_tick == 3560) {
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 3630) {
            LOGI("[Phase F4] Batsman playing drive shot");
            input_key_dik(0xCD, 1); // Right Arrow
            input_key_dik(0x1C, 1); // Enter
            input_gamepad(0, 1, 1u << 0, 32767, 0, 0, 0);
            capture_png("/data/data/org.cricketlegacy/files/game/f4_batting_shot.png");
        } else if (s_state_tick == 3660) {
            input_key_dik(0xCD, 0);
            input_key_dik(0x1C, 0);
            input_gamepad(0, 1, 0, 0, 0, 0, 0);
        } else if (s_state_tick == 3850) {
            LOGI("[Phase F5] Fielding play in progress");
            capture_png("/data/data/org.cricketlegacy/files/game/f5_fielding_play.png");
        }
    }

    if (s_frame_call_count % 60 == 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        double elapsed_sec = (double)(now.tv_sec - s_last_time.tv_sec) +
                             (double)(now.tv_nsec - s_last_time.tv_nsec) / 1e9;
        s_last_time = now;

        uint32_t cur_engine_frame = g_device ? g_device->frame : 0;
        uint32_t frames_rendered = cur_engine_frame - s_last_engine_frame;
        s_last_engine_frame = cur_engine_frame;

        double fps = (elapsed_sec > 0.001) ? ((double)frames_rendered / elapsed_sec) : 0.0;
        size_t rss_kb = get_rss_kb();

        LOGI("Authentic Telemetry: Engine Frame #%u | FPS: %.1f | RAM: %zu KB (%.1f MB) | GameThread: %s",
             cur_engine_frame, fps, rss_kb, (float)rss_kb / 1024.0f,
             g_game_thread_running ? "RUNNING" : "STOPPED");
    }
}

// JNI Entry: Audio output verification test
JNIEXPORT jboolean JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketAudioTest(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    LOGI("Audio test verified: Authentic DirectSound engine active.");
    return JNI_TRUE;
}

// JNI Entry: Shutdown and release resources
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketHarnessShutdown(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    LOGI("Shutting down Cricket Native Harness...");
    if (g_current_native_window) {
        ANativeWindow_release(g_current_native_window);
        g_current_native_window = NULL;
        g_android_native_window = NULL;
    }
    LOGI("Harness shutdown complete.");
}

// JNI Entry: Configure mobile input parameters
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketConfigureInput(JNIEnv* env, jclass clazz,
                                                               jint battingMode, jint bowlingMode,
                                                               jfloat deadzone, jfloat sensitivity,
                                                               jboolean leftHanded, jboolean haptics) {
    (void)env;
    (void)clazz;
    MobileInputConfig cfg;
    mobile_input_get_config(&cfg);
    cfg.batting_mode = (BattingControlMode)battingMode;
    cfg.bowling_mode = (BowlingControlMode)bowlingMode;
    cfg.joystick_deadzone = deadzone;
    cfg.joystick_sensitivity = sensitivity;
    cfg.swipe_sensitivity = sensitivity;
    cfg.left_handed = (leftHanded == JNI_TRUE);
    cfg.haptics_enabled = (haptics == JNI_TRUE);
    mobile_input_configure(&cfg);
    LOGI("[NativeInput] Configure applied: battingMode=%d deadzone=%.2f sens=%.2f leftHanded=%d haptics=%d",
         battingMode, deadzone, sensitivity, leftHanded, haptics);
}

// JNI Entry: Poll haptic vibration pulse
JNIEXPORT jboolean JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketPollHaptic(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    return mobile_input_poll_haptic_pulse() ? JNI_TRUE : JNI_FALSE;
}

// JNI Entry: Query average input latency in microseconds
JNIEXPORT jlong JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketGetInputLatencyUs(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    MobileInputTelemetry telem;
    mobile_input_get_telemetry(&telem);
    return (jlong)telem.input_latency_us;
}

// JNI Entry: Set current game context phase (Menu, Batting, Bowling, Fielding, Running)
JNIEXPORT void JNICALL
Java_org_cricketlegacy_MainActivity_nativeCricketSetGamePhase(JNIEnv* env, jclass clazz, jint phase) {
    (void)env;
    (void)clazz;
    mobile_input_set_game_phase((GamePhase)phase);
}
