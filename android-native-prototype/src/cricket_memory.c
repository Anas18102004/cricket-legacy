#include "cricket_memory.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "CricketMemory"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000 // Linux 4.17+ flag
#endif

bool cricket_memory_init(CricketVirtualArena* arena, size_t arena_size) {
    if (!arena) return false;
    
    // Default to 4GB virtual address arena
    if (arena_size == 0) {
        arena_size = 0x100000000ULL; // 4 Gigabytes
    }

    arena->base_address = NULL;
    arena->reserved_size = 0;
    arena->mode = ARENA_MODE_UNINITIALIZED;
    arena->is_initialized = false;

    // Strategy 1: Attempt low 32-bit mapping at 0x00010000 (just above page 0 NULL guard)
    // using MAP_FIXED_NOREPLACE so we NEVER clobber Android Bionic or linker mappings.
    void* requested_low_addr = (void*)0x00010000;
    size_t low_size = (arena_size > 0x00010000) ? (arena_size - 0x00010000) : arena_size;

    void* ptr = mmap(requested_low_addr, low_size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);

    if (ptr != MAP_FAILED && ptr == requested_low_addr) {
        LOGI("Strategy 1 SUCCESS: Mapped directly in lower 32-bit memory at %p (Size: %zu)", 
             ptr, low_size);
        arena->base_address = (void*)0x0; // Base is 0; raw 32-bit absolute addresses are directly valid!
        arena->reserved_size = arena_size;
        arena->mode = ARENA_MODE_FIXED_LOWER_4GB;
        arena->is_initialized = true;
        return true;
    }

    if (ptr != MAP_FAILED) {
        // If MAP_FIXED_NOREPLACE was not respected by older kernel, clean up
        munmap(ptr, low_size);
    }

    LOGW("Strategy 1 (Lower 32-bit fixed mapping) unavailable or collision detected (Errno: %d).", errno);
    LOGI("Activating Strategy 2: Contiguous 4GB Base-Relative Virtual Arena.");

    // Strategy 2: Allocate unconstrained contiguous 4GB virtual memory arena.
    // Using MAP_NORESERVE guarantees pages are not backed by physical RAM until written.
    ptr = mmap(NULL, arena_size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);

    if (ptr == MAP_FAILED) {
        LOGE("Strategy 2 CRITICAL FAILURE: Cannot allocate 4GB virtual arena! Errno: %d", errno);
        return false;
    }

    arena->base_address = ptr;
    arena->reserved_size = arena_size;
    arena->mode = ARENA_MODE_BASE_OFFSET;
    arena->is_initialized = true;

    LOGI("Strategy 2 SUCCESS: 4GB Virtual Arena allocated at base %p (Relative Offsetting active)", ptr);
    return true;
}

uint32_t cricket_read_u32(CricketVirtualArena* arena, uint32_t x86_addr) {
    if (!arena || !arena->is_initialized) return 0;
    return cricket_read_u32_fast(arena, x86_addr);
}

void cricket_write_u32(CricketVirtualArena* arena, uint32_t x86_addr, uint32_t value) {
    if (!arena || !arena->is_initialized) return;
    cricket_write_u32_fast(arena, x86_addr, value);
}

size_t cricket_get_resident_memory_kb(void) {
    FILE* fp = fopen("/proc/self/statm", "r");
    if (!fp) return 0;

    unsigned long size, resident, share, text, lib, data, dt;
    if (fscanf(fp, "%lu %lu %lu %lu %lu %lu %lu", 
               &size, &resident, &share, &text, &lib, &data, &dt) != 7) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
    return resident * page_size_kb;
}

void cricket_memory_cleanup(CricketVirtualArena* arena) {
    if (arena && arena->is_initialized) {
        if (arena->mode == ARENA_MODE_FIXED_LOWER_4GB) {
            munmap((void*)0x00010000, arena->reserved_size - 0x00010000);
            LOGI("Released Strategy 1 fixed lower 4GB arena at 0x00010000");
        } else if (arena->base_address != NULL) {
            munmap(arena->base_address, arena->reserved_size);
            LOGI("Released Strategy 2 virtual memory arena at %p", arena->base_address);
        }
        arena->base_address = NULL;
        arena->reserved_size = 0;
        arena->mode = ARENA_MODE_UNINITIALIZED;
        arena->is_initialized = false;
    }
}
