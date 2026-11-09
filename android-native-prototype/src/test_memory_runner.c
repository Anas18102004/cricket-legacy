#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cricket_memory.h"
#include "test_physics.h"

// Test 1: Memory Arena Initialization and Address Strategy Detection
static bool test_arena_init_and_strategy(CricketVirtualArena* arena) {
    printf("[TEST 1] Initializing 4GB Virtual Memory Arena...\n");
    if (!cricket_memory_init(arena, 0)) {
        printf("  FAIL: cricket_memory_init returned false!\n");
        return false;
    }

    printf("  Arena Base Address: %p\n", arena->base_address);
    printf("  Arena Reserved Size: %zu (0x%zx) bytes\n", arena->reserved_size, arena->reserved_size);
    if (arena->mode == ARENA_MODE_FIXED_LOWER_4GB) {
        printf("  Active Mode: ARENA_MODE_FIXED_LOWER_4GB (Low 32-bit direct pointers valid)\n");
    } else if (arena->mode == ARENA_MODE_BASE_OFFSET) {
        printf("  Active Mode: ARENA_MODE_BASE_OFFSET (Base-relative translation active)\n");
    } else {
        printf("  FAIL: Unknown arena mode!\n");
        return false;
    }
    printf("  PASS: Arena initialization successful.\n");
    return true;
}

// Test 2: Boundary Addresses Read/Write Verification
static bool test_boundary_addresses(CricketVirtualArena* arena) {
    printf("[TEST 2] Testing Boundary Address Accesses...\n");
    uint32_t test_addrs[] = {
        0x00010000, // Lowest mapped page (above page 0 NULL guard)
        0x00400000, // Standard Win32 PE .text base
        0x00800000, // Standard Win32 .data base
        0x10000000, // Typical Win32 game heap
        0x7FFFF000, // High user space
        0xFFFFFFF0  // Extreme 32-bit boundary
    };
    int count = sizeof(test_addrs) / sizeof(test_addrs[0]);

    for (int i = 0; i < count; ++i) {
        uint32_t addr = test_addrs[i];
        uint32_t write_val = 0xA1B2C3D4 ^ addr;
        cricket_write_u32(arena, addr, write_val);
        uint32_t read_val = cricket_read_u32(arena, addr);

        if (read_val != write_val) {
            printf("  FAIL: Boundary mismatch at 0x%08X! Wrote 0x%08X, read 0x%08X\n",
                   addr, write_val, read_val);
            return false;
        }
        printf("  Verified addr 0x%08X -> 0x%08X (OK)\n", addr, read_val);
    }
    printf("  PASS: Boundary address verification successful.\n");
    return true;
}

// Test 3: Stored Pointer Simulation (C++ VTable & Entity Linking)
static bool test_stored_pointers(CricketVirtualArena* arena) {
    printf("[TEST 3] Testing Stored 32-bit Pointers inside Game Memory...\n");
    // Simulate an object at 0x01500000 having a pointer to its vtable at 0x00850000
    uint32_t object_addr = 0x01500000;
    uint32_t vtable_addr = 0x00850000;
    uint32_t function_addr = 0x00420000;

    // Write function pointer into vtable[0]
    cricket_write_u32(arena, vtable_addr, function_addr);

    // Write vtable pointer into object->vptr
    cricket_write_u32(arena, object_addr, vtable_addr);

    // Read back through simulated indirection
    uint32_t retrieved_vtable = cricket_read_u32(arena, object_addr);
    uint32_t retrieved_fn = cricket_read_u32(arena, retrieved_vtable);

    if (retrieved_vtable != vtable_addr || retrieved_fn != function_addr) {
        printf("  FAIL: Pointer indirection failed! Retrieved fn: 0x%08X, expected 0x%08X\n",
               retrieved_fn, function_addr);
        return false;
    }
    printf("  Object (0x%08X) -> VTable (0x%08X) -> Fn (0x%08X)\n",
           object_addr, retrieved_vtable, retrieved_fn);
    printf("  PASS: Stored 32-bit pointer simulation successful.\n");
    return true;
}

// Test 4: Unaligned Memory Loads & Stores
static bool test_unaligned_memory(CricketVirtualArena* arena) {
    printf("[TEST 4] Testing Unaligned Memory Accesses...\n");
    // Test odd byte offsets (e.g. 0x00500001, 0x00500003)
    uint32_t odd_addrs[] = { 0x00500001, 0x00500002, 0x00500003, 0x00500005 };
    for (int i = 0; i < 4; ++i) {
        uint32_t addr = odd_addrs[i];
        uint32_t test_val = 0xFEEDFACE ^ i;
        cricket_write_u32(arena, addr, test_val);
        uint32_t read_val = cricket_read_u32(arena, addr);

        if (read_val != test_val) {
            printf("  FAIL: Unaligned mismatch at 0x%08X! Wrote 0x%08X, read 0x%08X\n",
                   addr, test_val, read_val);
            return false;
        }
        printf("  Unaligned addr 0x%08X -> 0x%08X (OK)\n", addr, read_val);
    }
    printf("  PASS: Unaligned memory accesses verified.\n");
    return true;
}

// Test 5: 32-bit Address Wraparound & Guard Page Protection
static bool test_address_wraparound(CricketVirtualArena* arena) {
    printf("[TEST 5] Testing 32-Bit Address Wraparound & Guard Protection...\n");
    // Arithmetic wraparound test
    uint32_t base = 0xFFFFFFFC;
    uint32_t offset = 8;
    uint32_t wrapped_addr = (uint32_t)(base + offset); // 0x00000004

    void* native_ptr_wrapped = cricket_translate_addr(arena, wrapped_addr);
    void* native_ptr_direct = cricket_translate_addr(arena, 4);

    if (native_ptr_wrapped != native_ptr_direct) {
        printf("  FAIL: Address wraparound mismatch! %p != %p\n",
               native_ptr_wrapped, native_ptr_direct);
        return false;
    }
    printf("  0xFFFFFFFC + 8 wraps to 0x%08X (Offset %p)\n", wrapped_addr, native_ptr_wrapped);

    // Guard page verification: ensure address < 0x00010000 is recognized as unmapped
    if (cricket_is_valid_guest_addr(wrapped_addr)) {
        printf("  FAIL: Low address 0x%08X was incorrectly flagged as valid guest memory!\n", wrapped_addr);
        return false;
    }
    printf("  Guard Page Check: 0x%08X safely flagged as unmapped/NULL-guard (NO dereference attempted)\n", wrapped_addr);

    // Wraparound into valid mapped space (.text at 0x00400000)
    uint32_t text_base_wrapped = (uint32_t)(0xFFFFFFFFU + 0x00400001U); // 0x00400000
    if (!cricket_is_valid_guest_addr(text_base_wrapped)) {
        printf("  FAIL: Wrapped .text base 0x%08X should be valid!\n", text_base_wrapped);
        return false;
    }
    uint32_t val_written = 0x55AA33CC;
    cricket_write_u32(arena, text_base_wrapped, val_written);
    uint32_t val_read = cricket_read_u32(arena, 0x00400000);
    if (val_read != val_written) {
        printf("  FAIL: Wrapped address write at 0x%08X read back 0x%08X (expected 0x%08X)\n",
               text_base_wrapped, val_read, val_written);
        return false;
    }
    printf("  Verified wraparound read/write into valid .text space (0x%08X -> 0x%08X)\n",
           text_base_wrapped, val_read);
    printf("  PASS: Address wraparound and guard protection verified.\n");
    return true;
}

// Test 6: Resident Memory (RSS) Profiling
static bool test_resident_memory_rss(void) {
    printf("[TEST 6] Testing Resident Memory (RSS) Monitoring...\n");
    size_t rss_kb = cricket_get_resident_memory_kb();
    printf("  Current Process Resident RAM: %zu KB (%.2f MB)\n", rss_kb, (float)rss_kb / 1024.0f);
    printf("  PASS: RSS profiling operational.\n");
    return true;
}

// Test 7: Ballistic Physics Determinism Test
static bool test_ballistic_physics(void) {
    printf("[TEST 7] Running ARM64 Ballistic Physics Validation Test...\n");
    if (!cricket_physics_run_validation_test()) {
        printf("  FAIL: Ballistic physics validation test failed!\n");
        return false;
    }
    printf("  PASS: Physics calculation determinism verified.\n");
    return true;
}

// Test 8: Memory Lifecycle & Clean Re-initialization
static bool test_memory_lifecycle(void) {
    printf("[TEST 8] Testing Memory Cleanup and Safe Re-allocation...\n");
    CricketVirtualArena temp_arena;
    if (!cricket_memory_init(&temp_arena, 0x10000000)) { // 256MB test
        printf("  FAIL: Temporary arena allocation failed!\n");
        return false;
    }
    cricket_write_u32(&temp_arena, 0x00400000, 0x12345678);
    cricket_memory_cleanup(&temp_arena);

    if (temp_arena.is_initialized || temp_arena.base_address != NULL) {
        printf("  FAIL: Arena cleanup did not reset fields!\n");
        return false;
    }
    printf("  PASS: Memory cleanup and deallocation verified.\n");
    return true;
}

int main(int argc, char** argv) {
    printf("====================================================\n");
    printf("CRICKET LEGACY — PHASE B.7 NATIVE TEST RUNNER\n");
    printf("Architectural & Memory Correctness Verification Suite\n");
    printf("====================================================\n\n");

    CricketVirtualArena arena;
    bool all_passed = true;

    all_passed &= test_arena_init_and_strategy(&arena);
    all_passed &= test_boundary_addresses(&arena);
    all_passed &= test_stored_pointers(&arena);
    all_passed &= test_unaligned_memory(&arena);
    all_passed &= test_address_wraparound(&arena);
    all_passed &= test_resident_memory_rss();
    all_passed &= test_ballistic_physics();
    all_passed &= test_memory_lifecycle();

    cricket_memory_cleanup(&arena);

    printf("\n====================================================\n");
    if (all_passed) {
        printf("PHASE B.7 TEST RESULT: ALL 8 TESTS PASSED SUCCESSFULLY!\n");
    } else {
        printf("PHASE B.7 TEST RESULT: ONE OR MORE TESTS FAILED!\n");
    }
    printf("====================================================\n");

    return all_passed ? 0 : 1;
}
