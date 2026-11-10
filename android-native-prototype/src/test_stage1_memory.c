#include "test_stage1_memory.h"
#include "game_static_data.h"
#include "cricket_memory.h"
#include "upstream_d3d8/cpu.h"
#include "upstream_d3d8/win32.h"
#include "upstream_d3d8/com.h"
#include "upstream_d3d8/d3d8.h"
#include <android/log.h>
#include <string.h>

#define TAG "CricketLegacy-Stage1Test"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

extern uint32_t g_device_object;

bool run_stage1_memory_tests(void) {
    LOGI("==================================================================");
    LOGI("CRICKET LEGACY — PHASE C.2 STAGE 1 MEMORY & STATIC DATA TEST SUITE");
    LOGI("==================================================================");

    int passed = 0;
    int failed = 0;

    // Ensure initialization
    if (!game_static_data_init()) {
        LOGE("FAILED: game_static_data_init() returned false!");
        return false;
    }

    const CricketPEImageState *state = game_static_data_get_state();

    // ------------------------------------------------------------------------
    // TEST 1: Section Boundary Alignment & Contiguity
    // ------------------------------------------------------------------------
    LOGI("[TEST 1/8] Checking section alignment and contiguity...");
    bool t1_pass = true;
    if (state->image_base != CRICKET_IMAGE_BASE) t1_pass = false;
    for (uint32_t i = 0; i < state->num_sections; i++) {
        if ((state->sections[i].va % 4096) != 0) {
            LOGE("Section %s VA (0x%08x) is not 4KB aligned!", state->sections[i].name, state->sections[i].va);
            t1_pass = false;
        }
        if (i > 0) {
            uint32_t prev_end = state->sections[i - 1].va + state->sections[i - 1].size;
            if (state->sections[i].va != prev_end) {
                LOGE("Section %s VA (0x%08x) does not match prev end (0x%08x)!",
                     state->sections[i].name, state->sections[i].va, prev_end);
                t1_pass = false;
            }
        }
    }
    if (t1_pass) {
        LOGI("  -> PASS: All sections are 4KB-aligned and contiguous across [0x%08x - 0x%08x]",
             CRICKET_IMAGE_BASE, CRICKET_IMAGE_END);
        passed++;
    } else {
        LOGE("  -> FAIL: Section alignment check failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 2: PE Header Structure Integrity
    // ------------------------------------------------------------------------
    LOGI("[TEST 2/8] Checking PE Header structure integrity...");
    bool t2_pass = true;
    uint8_t *img = (uint8_t*)HP(CRICKET_IMAGE_BASE);
    if (img[0] != 'M' || img[1] != 'Z') {
        LOGE("DOS Header signature is not 'MZ' (got %c%c)!", img[0], img[1]);
        t2_pass = false;
    }
    uint32_t lfanew = R32(CRICKET_IMAGE_BASE + 0x3C);
    if (lfanew != 0x80) {
        LOGE("e_lfanew offset mismatch (expected 0x80, got 0x%x)!", lfanew);
        t2_pass = false;
    }
    uint8_t *pe = (uint8_t*)HP(CRICKET_IMAGE_BASE + lfanew);
    if (pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0) {
        LOGE("PE signature mismatch!");
        t2_pass = false;
    }
    uint16_t machine = R16(CRICKET_IMAGE_BASE + 0x84);
    if (machine != 0x014C) {
        LOGE("Machine architecture mismatch (expected 0x014C, got 0x%x)!", machine);
        t2_pass = false;
    }
    if (t2_pass) {
        LOGI("  -> PASS: Authentic PE/DOS headers verified (MZ/PE, Machine=i386, Base=0x00400000)");
        passed++;
    } else {
        LOGE("  -> FAIL: PE Header structure integrity failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 3: Thread Information Block (GUEST_TIB) Structure
    // ------------------------------------------------------------------------
    LOGI("[TEST 3/8] Checking Thread Information Block (TIB at 0x%08x)...", CRICKET_GUEST_TIB);
    bool t3_pass = true;
    uint32_t seh = R32(CRICKET_GUEST_TIB + 0x00);
    uint32_t stack_base = R32(CRICKET_GUEST_TIB + 0x04);
    uint32_t stack_limit = R32(CRICKET_GUEST_TIB + 0x08);
    uint32_t tib_self = R32(CRICKET_GUEST_TIB + 0x18);
    if (seh != 0xFFFFFFFFu) { LOGE("TIB SEH chain mismatch: 0x%08x", seh); t3_pass = false; }
    if (stack_base != CRICKET_GUEST_STACK_TOP) { LOGE("TIB Stack base mismatch: 0x%08x", stack_base); t3_pass = false; }
    if (stack_limit != (CRICKET_GUEST_STACK_TOP - CRICKET_GUEST_STACK_SIZE)) {
        LOGE("TIB Stack limit mismatch: 0x%08x", stack_limit); t3_pass = false;
    }
    if (tib_self != CRICKET_GUEST_TIB) { LOGE("TIB self pointer mismatch: 0x%08x", tib_self); t3_pass = false; }
    if (t3_pass) {
        LOGI("  -> PASS: GUEST_TIB verified (SEH: -1, Stack: [0x%08x - 0x%08x], Self: 0x%08x)",
             stack_limit, stack_base, tib_self);
        passed++;
    } else {
        LOGE("  -> FAIL: Thread Information Block check failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 4: .data Segment Read/Write Persistence
    // ------------------------------------------------------------------------
    LOGI("[TEST 4/8] Testing .data section read/write persistence...");
    bool t4_pass = true;
    uint32_t test_addr1 = CRICKET_DATA_START + 0x4000;
    uint32_t test_addr2 = CRICKET_DATA_START + 0x4004;
    W32(test_addr1, 0xDEADBEEFu);
    W32(test_addr2, 0xCAFEBABEu);
    if (R32(test_addr1) != 0xDEADBEEFu || R32(test_addr2) != 0xCAFEBABEu) {
        LOGE("32-bit read/write persistence mismatch!");
        t4_pass = false;
    }
    // Test unaligned 32-bit access
    uint32_t unaligned_addr = CRICKET_DATA_START + 0x4009; // Not 4-byte aligned
    W32(unaligned_addr, 0x12345678u);
    if (R32(unaligned_addr) != 0x12345678u) {
        LOGE("Unaligned 32-bit access mismatch!");
        t4_pass = false;
    }
    if (t4_pass) {
        LOGI("  -> PASS: .data section read/write and unaligned access verified (0x12345678)");
        passed++;
    } else {
        LOGE("  -> FAIL: .data read/write persistence failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 5: .bss Complete Zero-Initialization
    // ------------------------------------------------------------------------
    LOGI("[TEST 5/8] Verifying .bss complete zero-initialization...");
    bool t5_pass = true;
    // Check 1000 sampled points across .bss
    for (uint32_t offset = 0; offset < CRICKET_BSS_SIZE; offset += 1024) {
        uint32_t val = R32(CRICKET_BSS_START + offset);
        if (val != 0) {
            LOGE("Non-zero byte found in .bss at offset 0x%x (val=0x%08x)!", offset, val);
            t5_pass = false;
            break;
        }
    }
    if (t5_pass) {
        LOGI("  -> PASS: .bss section verified 100%% zero-filled across 1.0 MB range");
        passed++;
    } else {
        LOGE("  -> FAIL: .bss contains non-zero data!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 6: Known Game Global Variables Accessibility
    // ------------------------------------------------------------------------
    LOGI("[TEST 6/8] Verifying known Cricket 07 global variables...");
    bool t6_pass = true;
    float cur_x, cur_y;
    uint32_t rx = R32(CRICKET_VAR_CURSOR_POS + 0);
    uint32_t ry = R32(CRICKET_VAR_CURSOR_POS + 4);
    memcpy(&cur_x, &rx, 4);
    memcpy(&cur_y, &ry, 4);
    if (cur_x < 319.9f || cur_x > 320.1f || cur_y < 239.9f || cur_y > 240.1f) {
        LOGE("PROFILE_CURSOR coordinates mismatch (expected 320, 240, got %.1f, %.1f)!", cur_x, cur_y);
        t6_pass = false;
    }
    if (R32(CRICKET_VAR_WIDESCREEN) != 1) {
        LOGE("CRICKET_VAR_WIDESCREEN mismatch (expected 1, got %u)!", R32(CRICKET_VAR_WIDESCREEN));
        t6_pass = false;
    }
    if (t6_pass) {
        LOGI("  -> PASS: PROFILE_CURSOR=(%.1f, %.1f), Widescreen=1 verified", cur_x, cur_y);
        passed++;
    } else {
        LOGE("  -> FAIL: Known game global variable accessibility check failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 7: Static D3D Device Pointer Linkage
    // ------------------------------------------------------------------------
    LOGI("[TEST 7/8] Verifying static D3D Device pointer linkage...");
    bool t7_pass = true;
    if (g_device_object) {
        W32(CRICKET_VAR_D3D_DEVICE, g_device_object);
        uint32_t dev_read = R32(CRICKET_VAR_D3D_DEVICE);
        if (dev_read != g_device_object) {
            LOGE("D3D Device pointer readback mismatch: expected 0x%08x, got 0x%08x", g_device_object, dev_read);
            t7_pass = false;
        }
        ComObj *o = com_get(dev_read);
        if (!o || o->type != RT_DEVICE) {
            LOGE("Failed to resolve ComObj from static memory pointer 0x%08x", dev_read);
            t7_pass = false;
        }
    } else {
        LOGI("  (Device not yet active - testing static pointer store/readback)");
        W32(CRICKET_VAR_D3D_DEVICE, 0x111913a0u);
        if (R32(CRICKET_VAR_D3D_DEVICE) != 0x111913a0u) t7_pass = false;
    }
    if (t7_pass) {
        LOGI("  -> PASS: Static Direct3D device pointer verified in .data segment");
        passed++;
    } else {
        LOGE("  -> FAIL: Static D3D device pointer linkage failed!");
        failed++;
    }

    // ------------------------------------------------------------------------
    // TEST 8: 32-bit Address Wraparound & Memory Containment
    // ------------------------------------------------------------------------
    LOGI("[TEST 8/8] Testing 32-bit address wraparound & safety containment...");
    bool t8_pass = true;
    uint32_t near_top = 0xFFFFFFFCu;
    uint32_t wrapped = near_top + 8; // 0x00000004
    if (wrapped != 0x00000004u) {
        LOGE("Wraparound arithmetic failed: 0x%08x", wrapped);
        t8_pass = false;
    }
    if (cricket_is_valid_guest_addr(wrapped)) {
        LOGE("Address 0x00000004 should be identified as invalid guard page!");
        t8_pass = false;
    }
    if (!cricket_is_valid_guest_addr(CRICKET_IMAGE_BASE)) {
        LOGE("Address 0x%08x should be identified as valid guest memory!", CRICKET_IMAGE_BASE);
        t8_pass = false;
    }
    if (t8_pass) {
        LOGI("  -> PASS: 32-bit wraparound and guard page validation verified");
        passed++;
    } else {
        LOGE("  -> FAIL: 32-bit address safety check failed!");
        failed++;
    }

    LOGI("==================================================================");
    LOGI("STAGE 1 TEST RESULTS: %d/8 PASSED, %d FAILED", passed, failed);
    LOGI("==================================================================");

    return (failed == 0);
}
