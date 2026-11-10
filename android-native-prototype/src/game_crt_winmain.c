#include "game_crt_winmain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "CricketCRT"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define FAKE_MODULE(i) (0xF1000000u + (i) * 0x10000u)
static const char *g_known_dlls[] = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "winmm.dll",
    "d3d8.dll", "ddraw.dll", "dinput.dll", "dsound.dll", "version.dll", NULL
};

static uint32_t g_cmdline = 0;
static uint32_t g_cmdline_w = 0;
static uint32_t g_env = 0;
static pthread_mutex_t g_crt_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_crt_initialized = false;

// ------------------------------------------------------------------ Guest Memory String Duplication

uint32_t guest_strdup(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s) + 1;
    uint32_t a = heap_alloc((uint32_t)len, 0);
    if (a) {
        memcpy(HP(a), s, len);
    }
    return a;
}

uint32_t module_for(const char *name) {
    if (!name || !*name) return g_image_lo ? g_image_lo : 0x00400000u;
    const char *b = strrchr(name, '\\');
    b = b ? b + 1 : name;
    for (int i = 0; g_known_dlls[i]; i++) {
        if (!strncasecmp(b, g_known_dlls[i], strlen(g_known_dlls[i]) - 4)) {
            return FAKE_MODULE(i + 1);
        }
    }
    return 0;
}

// ------------------------------------------------------------------ Subsystem Lifecycle

bool crt_startup_init(const char *cmd_line, const char *exe_name) {
    pthread_mutex_lock(&g_crt_lock);
    char full_cmd[1024];
    if (cmd_line && strlen(cmd_line) > 0) {
        snprintf(full_cmd, sizeof(full_cmd), "%s", cmd_line);
    } else {
        snprintf(full_cmd, sizeof(full_cmd), "\"%s\\%s\"", PROFILE_GUEST_ROOT, 
                 exe_name ? exe_name : PROFILE_GUEST_EXE);
    }

    g_cmdline = guest_strdup(full_cmd);

    // Prepare wide-char UTF-16 command line
    size_t cmd_len = strlen(full_cmd);
    g_cmdline_w = heap_alloc((uint32_t)((cmd_len + 1) * 2), 1);
    if (g_cmdline_w) {
        uint16_t *wp = (uint16_t *)HP(g_cmdline_w);
        for (size_t i = 0; i <= cmd_len; i++) {
            wp[i] = (uint16_t)(unsigned char)full_cmd[i];
        }
    }

    // Prepare environment block: "PATH=C:\WINDOWS\0SYSTEMROOT=C:\WINDOWS\0\0"
    const char env_block[] = "PATH=C:\\WINDOWS\0SYSTEMROOT=C:\\WINDOWS\0";
    size_t env_sz = sizeof(env_block) + 1;
    g_env = heap_alloc((uint32_t)env_sz, 1);
    if (g_env) {
        memcpy(HP(g_env), env_block, sizeof(env_block));
    }

    g_crt_initialized = true;
    pthread_mutex_unlock(&g_crt_lock);

    LOGI("CRT Startup Subsystem Initialized. CmdLine: %s, ImageBase: 0x%08X", full_cmd, g_image_lo);
    return true;
}

void crt_startup_shutdown(void) {
    pthread_mutex_lock(&g_crt_lock);
    if (g_cmdline) { heap_free(g_cmdline); g_cmdline = 0; }
    if (g_cmdline_w) { heap_free(g_cmdline_w); g_cmdline_w = 0; }
    if (g_env) { heap_free(g_env); g_env = 0; }
    g_crt_initialized = false;
    pthread_mutex_unlock(&g_crt_lock);
    LOGI("CRT Startup Subsystem shut down.");
}

// ------------------------------------------------------------------ Win32 Guest stdcall Handlers

void w_GetModuleHandleA(CPU *c) {
    uint32_t p = ARG(0);
    const char *name = p ? GSTR(p) : NULL;
    uint32_t m = module_for(name);
    RET(1, m);
}

void w_GetCommandLineA(CPU *c) {
    if (!g_cmdline) {
        crt_startup_init(NULL, NULL);
    }
    RET(0, g_cmdline);
}

void w_GetCommandLineW(CPU *c) {
    if (!g_cmdline_w) {
        crt_startup_init(NULL, NULL);
    }
    RET(0, g_cmdline_w);
}

void w_GetStartupInfoA(CPU *c) {
    uint32_t p = ARG(0);
    if (p) {
        memset(HP(p), 0, sizeof(Win32StartupInfoA));
        W32(p, sizeof(Win32StartupInfoA)); // cb = 68 bytes
    }
    RET(1, 0);
}

void w_GetEnvironmentStrings(CPU *c) {
    if (!g_env) {
        crt_startup_init(NULL, NULL);
    }
    RET(0, g_env);
}

void w_FreeEnvironmentStringsA(CPU *c) {
    RET(1, 1);
}

void w_GetVersionExA(CPU *c) {
    uint32_t p = ARG(0);
    if (p) {
        W32(p + 4, 5);    // dwMajorVersion = 5 (Windows XP)
        W32(p + 8, 1);    // dwMinorVersion = 1
        W32(p + 12, 2600);// dwBuildNumber = 2600
        W32(p + 16, VER_PLATFORM_WIN32_NT);
        strcpy((char *)HP(p + 20), "Service Pack 2");
    }
    RET(1, 1);
}

void w_GetProcessHeap(CPU *c) {
    RET(0, HEAP_HANDLE);
}

void w_HeapAlloc(CPU *c) {
    uint32_t flags = ARG(1);
    uint32_t size = ARG(2);
    uint32_t a = heap_alloc(size, (flags & 8) ? 1 : 0);
    RET(3, a);
}

void w_HeapFree(CPU *c) {
    uint32_t a = ARG(2);
    heap_free(a);
    RET(3, 1);
}

void w_HeapReAlloc(CPU *c) {
    uint32_t flags = ARG(1);
    uint32_t a = ARG(2);
    uint32_t size = ARG(3);
    uint32_t b = heap_realloc(a, size, (flags & 8) ? 1 : 0);
    RET(4, b);
}

void w_GetStdHandle(CPU *c) {
    int32_t nStdHandle = (int32_t)ARG(0);
    uint32_t h = 0;
    if (nStdHandle == -10) h = 0xFFFFFFF6u;      // STD_INPUT_HANDLE
    else if (nStdHandle == -11) h = 0xFFFFFFF5u; // STD_OUTPUT_HANDLE
    else if (nStdHandle == -12) h = 0xFFFFFFF4u; // STD_ERROR_HANDLE
    else h = 0xFFFFFFFFu;
    RET(1, h);
}

void w_ExitProcess(CPU *c) {
    uint32_t code = ARG(0);
    LOGI("[Win32] ExitProcess(%u) requested by thread", code);
    c->eax = code;
    // stdcall clean return
    c->esp += 4;
}

// ------------------------------------------------------------------ Dedicated WinMain Launcher

typedef struct {
    GuestFn fn;
    uint32_t image_base;
    int n_cmd_show;
    int exit_code;
    bool completed;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} WinMainThreadContext;

static void *winmain_thread_worker(void *arg) {
    WinMainThreadContext *ctx = (WinMainThreadContext *)arg;
    LOGI("[GameThread] Worker thread started with 64MB host stack.");

    // Allocate thread record
    GuestThread thread_rec;
    memset(&thread_rec, 0, sizeof(thread_rec));
    thread_rec.id = 0x100;
    thread_rec.tib = 0x00010000u; // Match GUEST_TIB base from Stage 1
    t_self = &thread_rec;

    CPU *c = &thread_rec.cpu;
    memset(c, 0, sizeof(CPU));

    // Setup initial CPU state for standard Win32 execution
    c->fs_base = thread_rec.tib;
    c->fpu_cw = 0x027F; // Standard x87 default control word
    c->mxcsr = 0x1F80;  // Standard SSE default control word

    // Stack configuration: Top of emulated stack at 0x002FFFF0
    c->esp = 0x002FFFF0u;
    c->ebp = c->esp;

    // Push stdcall WinMain arguments right-to-left:
    // int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
    // 4. nCmdShow
    c->esp -= 4;
    W32(c->esp, (uint32_t)ctx->n_cmd_show);
    // 3. lpCmdLine
    c->esp -= 4;
    W32(c->esp, g_cmdline);
    // 2. hPrevInstance (always NULL in Win32)
    c->esp -= 4;
    W32(c->esp, 0);
    // 1. hInstance (Image Base e.g. 0x00400000)
    c->esp -= 4;
    W32(c->esp, ctx->image_base);

    // Push simulated return address (sentinel)
    c->esp -= 4;
    W32(c->esp, GUEST_RETURN_SENTINEL);

    LOGI("[GameThread] Stack prepared: esp=0x%08X. Entering entry trampoline...", c->esp);

    // Call the function
    ctx->fn(c);

    int exit_code = (int)c->eax;
    LOGI("[GameThread] Trampoline execution completed. ExitCode: 0x%08X (%d)", exit_code, exit_code);

    pthread_mutex_lock(&ctx->lock);
    ctx->exit_code = exit_code;
    ctx->completed = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    return NULL;
}

bool crt_launch_winmain(GuestFn winmain_fn, uint32_t image_base, int n_cmd_show, int *out_exit_code) {
    if (!winmain_fn) return false;

    WinMainThreadContext ctx;
    ctx.fn = winmain_fn;
    ctx.image_base = image_base ? image_base : 0x00400000u;
    ctx.n_cmd_show = n_cmd_show;
    ctx.exit_code = -1;
    ctx.completed = false;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // 64 MB host stack for nested recompiled execution frames
    pthread_attr_setstacksize(&attr, 64 * 1024 * 1024);

    pthread_t thread;
    int err = pthread_create(&thread, &attr, winmain_thread_worker, &ctx);
    pthread_attr_destroy(&attr);

    if (err != 0) {
        LOGE("Failed to create WinMain pthread! Error: %d", err);
        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.cond);
        return false;
    }

    // Wait for thread to finish
    pthread_mutex_lock(&ctx.lock);
    while (!ctx.completed) {
        pthread_cond_wait(&ctx.cond, &ctx.lock);
    }
    pthread_mutex_unlock(&ctx.lock);

    pthread_join(thread, NULL);
    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.cond);

    if (out_exit_code) {
        *out_exit_code = ctx.exit_code;
    }
    return true;
}

// ------------------------------------------------------------------ Clearly Labeled Synthetic Test Program

void test_crt_winmain_program(CPU *c) {
    // Stage 3 Validation Harness:
    // Verify standard WinMain ABI stdcall calling convention:
    // [esp + 4]  = hInstance
    // [esp + 8]  = hPrevInstance
    // [esp + 12] = lpCmdLine
    // [esp + 16] = nCmdShow
    uint32_t hInstance = ARG(0);
    uint32_t hPrevInstance = ARG(1);
    uint32_t lpCmdLine = ARG(2);
    uint32_t nCmdShow = ARG(3);

    LOGI("  [SYNTHETIC TEST PROGRAM] WinMain Entry reached!");
    LOGI("    ARG(0) hInstance     : 0x%08X", hInstance);
    LOGI("    ARG(1) hPrevInstance : 0x%08X", hPrevInstance);
    LOGI("    ARG(2) lpCmdLine     : 0x%08X ('%s')", lpCmdLine, lpCmdLine ? GSTR(lpCmdLine) : "NULL");
    LOGI("    ARG(3) nCmdShow      : %u", nCmdShow);

    // Verify ABI assertions
    if (hInstance != 0x00400000u) {
        LOGE("    FAIL: hInstance mismatch! Expected 0x00400000, got 0x%08X", hInstance);
        RET(4, 0xDEAD0001);
    }
    if (hPrevInstance != 0) {
        LOGE("    FAIL: hPrevInstance must be NULL! Got 0x%08X", hPrevInstance);
        RET(4, 0xDEAD0002);
    }
    if (!lpCmdLine || strstr(GSTR(lpCmdLine), "Cricket07.exe") == NULL) {
        LOGE("    FAIL: lpCmdLine missing 'Cricket07.exe' substring!");
        RET(4, 0xDEAD0003);
    }
    if (nCmdShow != 10) { // SW_SHOWDEFAULT
        LOGE("    FAIL: nCmdShow mismatch! Expected 10, got %u", nCmdShow);
        RET(4, 0xDEAD0004);
    }

    // Exercise dynamic heap allocation inside test program
    uint32_t test_heap_ptr = heap_alloc(128, 1);
    if (!test_heap_ptr) {
        LOGE("    FAIL: Heap allocation failed inside test program!");
        RET(4, 0xDEAD0005);
    }
    strcpy((char *)HP(test_heap_ptr), "CRICKET_LEGACY_TEST_STAGE3_OK");
    if (strcmp((const char *)HP(test_heap_ptr), "CRICKET_LEGACY_TEST_STAGE3_OK") != 0) {
        LOGE("    FAIL: Heap memory write/read mismatch!");
        heap_free(test_heap_ptr);
        RET(4, 0xDEAD0006);
    }
    heap_free(test_heap_ptr);

    LOGI("  [SYNTHETIC TEST PROGRAM] All ABI and runtime checks passed! Returning 0xCAFE0007.");
    // WinMain returns 0xCAFE0007 as proof of successful execution
    RET(4, 0xCAFE0007);
}
