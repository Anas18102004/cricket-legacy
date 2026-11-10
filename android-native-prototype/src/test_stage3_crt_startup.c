#include "test_stage3_crt_startup.h"
#include "game_crt_winmain.h"
#include "upstream_d3d8/win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "Stage3CRTTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool test1_command_line(void) {
    LOGI("[TEST 1] Testing Win32 Command Line Setup (GetCommandLineA/W)...");

    CPU c;
    memset(&c, 0, sizeof(CPU));
    w_GetCommandLineA(&c);

    uint32_t cmd_ptr = c.eax;
    if (!cmd_ptr) {
        LOGE("  FAIL: GetCommandLineA returned NULL pointer!");
        return false;
    }

    const char *cmd_str = (const char *)HP(cmd_ptr);
    LOGI("  GetCommandLineA: '%s' (Guest Address: 0x%08X)", cmd_str, cmd_ptr);
    if (!strstr(cmd_str, "Cricket07.exe") || !strstr(cmd_str, "C:\\Cricket07")) {
        LOGE("  FAIL: Command line does not contain expected executable path!");
        return false;
    }

    w_GetCommandLineW(&c);
    uint32_t cmd_w_ptr = c.eax;
    if (!cmd_w_ptr) {
        LOGE("  FAIL: GetCommandLineW returned NULL pointer!");
        return false;
    }
    const uint16_t *wp = (const uint16_t *)HP(cmd_w_ptr);
    if (wp[0] != '"' && wp[0] != 'C') {
        LOGE("  FAIL: GetCommandLineW first char invalid: 0x%04X", wp[0]);
        return false;
    }

    LOGI("  PASS: Test 1 Command Line verified.");
    return true;
}

static bool test2_startup_info(void) {
    LOGI("[TEST 2] Testing Win32 Startup Info (GetStartupInfoA)...");

    // Allocate 128 bytes in guest memory for STARTUPINFOA
    uint32_t guest_si = heap_alloc(128, 1);
    if (!guest_si) {
        LOGE("  FAIL: Failed to allocate guest buffer for STARTUPINFOA!");
        return false;
    }

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, guest_si); // ARG(0) = pointer to STARTUPINFOA

    w_GetStartupInfoA(&c);

    uint32_t cb = R32(guest_si);
    heap_free(guest_si);

    LOGI("  GetStartupInfoA cb: %u bytes (Expected %zu)", cb, sizeof(Win32StartupInfoA));
    if (cb != sizeof(Win32StartupInfoA)) {
        LOGE("  FAIL: STARTUPINFOA cb mismatch! Expected %zu, got %u", sizeof(Win32StartupInfoA), cb);
        return false;
    }

    LOGI("  PASS: Test 2 Startup Info verified.");
    return true;
}

static bool test3_module_handles(void) {
    LOGI("[TEST 3] Testing Module Handle Resolution (GetModuleHandleA)...");

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;

    // 3a. GetModuleHandleA(NULL) -> ImageBase (0x00400000)
    W32(c.esp + 4, 0); // ARG(0) = NULL
    w_GetModuleHandleA(&c);
    uint32_t main_mod = c.eax;
    LOGI("  GetModuleHandleA(NULL) -> 0x%08X", main_mod);
    if (main_mod != 0x00400000u) {
        LOGE("  FAIL: Expected image base 0x00400000, got 0x%08X!", main_mod);
        return false;
    }

    // 3b. GetModuleHandleA("kernel32.dll") -> fake module
    uint32_t k32_str = guest_strdup("kernel32.dll");
    W32(c.esp + 4, k32_str);
    w_GetModuleHandleA(&c);
    uint32_t k32_mod = c.eax;
    heap_free(k32_str);
    LOGI("  GetModuleHandleA('kernel32.dll') -> 0x%08X", k32_mod);
    if (k32_mod < 0xF1000000u) {
        LOGE("  FAIL: Expected fake module >= 0xF1000000, got 0x%08X!", k32_mod);
        return false;
    }

    // 3c. GetModuleHandleA("nonexistent.dll") -> 0
    uint32_t non_str = guest_strdup("nonexistent.dll");
    W32(c.esp + 4, non_str);
    w_GetModuleHandleA(&c);
    uint32_t non_mod = c.eax;
    heap_free(non_str);
    LOGI("  GetModuleHandleA('nonexistent.dll') -> 0x%08X", non_mod);
    if (non_mod != 0) {
        LOGE("  FAIL: Expected 0 for nonexistent module, got 0x%08X!", non_mod);
        return false;
    }

    LOGI("  PASS: Test 3 Module Handles verified.");
    return true;
}

static bool test4_environment_strings(void) {
    LOGI("[TEST 4] Testing Win32 Environment Block (GetEnvironmentStrings)...");

    CPU c;
    memset(&c, 0, sizeof(CPU));
    w_GetEnvironmentStrings(&c);

    uint32_t env_ptr = c.eax;
    if (!env_ptr) {
        LOGE("  FAIL: GetEnvironmentStrings returned NULL!");
        return false;
    }

    const char *env_str = (const char *)HP(env_ptr);
    LOGI("  Environment Block at 0x%08X: '%s'", env_ptr, env_str);
    if (!strstr(env_str, "PATH=") && !strstr(env_str, "C:\\WINDOWS")) {
        LOGE("  FAIL: Environment block missing PATH or Windows directory!");
        return false;
    }

    w_FreeEnvironmentStringsA(&c);
    if (c.eax != 1) {
        LOGE("  FAIL: FreeEnvironmentStringsA returned failure!");
        return false;
    }

    LOGI("  PASS: Test 4 Environment Strings verified.");
    return true;
}

static bool test5_version_info(void) {
    LOGI("[TEST 5] Testing Win32 OS Version (GetVersionExA)...");

    uint32_t guest_vi = heap_alloc(sizeof(Win32OSVersionInfoA) + 16, 1);
    if (!guest_vi) {
        LOGE("  FAIL: Failed to allocate guest buffer for OSVERSIONINFOA!");
        return false;
    }

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, guest_vi); // ARG(0) = pointer to OSVERSIONINFOA

    w_GetVersionExA(&c);
    if (c.eax != 1) {
        LOGE("  FAIL: GetVersionExA returned failure!");
        heap_free(guest_vi);
        return false;
    }

    uint32_t major = R32(guest_vi + 4);
    uint32_t minor = R32(guest_vi + 8);
    uint32_t build = R32(guest_vi + 12);
    uint32_t plat = R32(guest_vi + 16);
    const char *csd = (const char *)HP(guest_vi + 20);

    LOGI("  Windows Version: %u.%u (Build %u), PlatformId: %u, CSD: '%s'",
         major, minor, build, plat, csd);

    heap_free(guest_vi);

    if (major != 5 || minor != 1 || build != 2600 || plat != VER_PLATFORM_WIN32_NT) {
        LOGE("  FAIL: OSVERSIONINFOA values do not match expected Windows XP SP2 NT profile!");
        return false;
    }

    LOGI("  PASS: Test 5 Version Info verified.");
    return true;
}

static bool test6_process_heap(void) {
    LOGI("[TEST 6] Testing Process Heap & Dynamic Allocations (GetProcessHeap, HeapAlloc)...");

    CPU c;
    memset(&c, 0, sizeof(CPU));
    w_GetProcessHeap(&c);

    uint32_t heap_handle = c.eax;
    LOGI("  GetProcessHeap() -> 0x%08X", heap_handle);
    if (heap_handle != HEAP_HANDLE) {
        LOGE("  FAIL: GetProcessHeap mismatch! Expected 0x%08X, got 0x%08X", HEAP_HANDLE, heap_handle);
        return false;
    }

    // Allocate 512 bytes via HeapAlloc stdcall wrapper
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, heap_handle); // ARG(0) = hHeap
    W32(c.esp + 8, 8);           // ARG(1) = HEAP_ZERO_MEMORY
    W32(c.esp + 12, 512);        // ARG(2) = dwBytes
    w_HeapAlloc(&c);

    uint32_t alloc_ptr = c.eax;
    LOGI("  HeapAlloc(512) -> 0x%08X", alloc_ptr);
    if (alloc_ptr < GUEST_HEAP_BASE || alloc_ptr >= GUEST_HEAP_HI) {
        LOGE("  FAIL: HeapAlloc returned pointer outside guest heap bounds!");
        return false;
    }

    // Verify memory was zero-initialized
    const uint8_t *mem = (const uint8_t *)HP(alloc_ptr);
    for (int i = 0; i < 512; i++) {
        if (mem[i] != 0) {
            LOGE("  FAIL: HeapAlloc byte %d was non-zero (0x%02X)!", i, mem[i]);
            return false;
        }
    }

    // Free via HeapFree stdcall wrapper
    c.esp = 0x002FFFF0u;
    W32(c.esp + 4, heap_handle);
    W32(c.esp + 8, 0);
    W32(c.esp + 12, alloc_ptr);
    w_HeapFree(&c);
    if (c.eax != 1) {
        LOGE("  FAIL: HeapFree returned failure!");
        return false;
    }

    LOGI("  PASS: Test 6 Process Heap verified.");
    return true;
}

static bool test7_std_handles(void) {
    LOGI("[TEST 7] Testing Standard Device Handles (GetStdHandle)...");

    CPU c;
    memset(&c, 0, sizeof(CPU));
    c.esp = 0x002FFFF0u;

    // STD_INPUT_HANDLE (-10)
    W32(c.esp + 4, (uint32_t)-10);
    w_GetStdHandle(&c);
    uint32_t h_in = c.eax;

    // STD_OUTPUT_HANDLE (-11)
    W32(c.esp + 4, (uint32_t)-11);
    w_GetStdHandle(&c);
    uint32_t h_out = c.eax;

    // STD_ERROR_HANDLE (-12)
    W32(c.esp + 4, (uint32_t)-12);
    w_GetStdHandle(&c);
    uint32_t h_err = c.eax;

    LOGI("  Std Handles: IN=0x%08X, OUT=0x%08X, ERR=0x%08X", h_in, h_out, h_err);
    if (h_in != 0xFFFFFFF6u || h_out != 0xFFFFFFF5u || h_err != 0xFFFFFFF4u) {
        LOGE("  FAIL: Standard handle constant mapping mismatch!");
        return false;
    }

    LOGI("  PASS: Test 7 Standard Handles verified.");
    return true;
}

static bool test8_winmain_launch_trampoline(void) {
    LOGI("[TEST 8] Testing WinMain Dedicated Thread Trampoline with Synthetic Test Program...");
    LOGI("  NOTE: Executing clearly labeled 'test_crt_winmain_program' (NOT actual Cricket 07 code).");

    int exit_code = 0;
    bool launched = crt_launch_winmain(test_crt_winmain_program, 0x00400000u, 10 /* SW_SHOWDEFAULT */, &exit_code);

    if (!launched) {
        LOGE("  FAIL: crt_launch_winmain failed to launch thread!");
        return false;
    }

    LOGI("  WinMain execution completed with ExitCode: 0x%08X", (uint32_t)exit_code);
    if ((uint32_t)exit_code != 0xCAFE0007u) {
        LOGE("  FAIL: Expected synthetic test exit code 0xCAFE0007, got 0x%08X!", (uint32_t)exit_code);
        return false;
    }

    LOGI("  PASS: Test 8 WinMain Launch Trampoline verified.");
    return true;
}

bool run_stage3_crt_startup_tests(void) {
    LOGI("==================================================================");
    LOGI("PHASE C.2 — STAGE 3: CRT STARTUP & WINMAIN TRAMPOLINE VERIFICATION");
    LOGI("MSVC CRT Initialization, Command Line & Game Thread Trampoline");
    LOGI("==================================================================");

    crt_startup_init(NULL, NULL);

    int passed = 0;
    int total = 8;

    if (test1_command_line()) passed++;
    if (test2_startup_info()) passed++;
    if (test3_module_handles()) passed++;
    if (test4_environment_strings()) passed++;
    if (test5_version_info()) passed++;
    if (test6_process_heap()) passed++;
    if (test7_std_handles()) passed++;
    if (test8_winmain_launch_trampoline()) passed++;

    LOGI("==================================================================");
    LOGI("STAGE 3 TEST SUMMARY: %d / %d TESTS PASSED (Failed: %d)", passed, total, total - passed);
    LOGI("==================================================================");

    return (passed == total);
}
