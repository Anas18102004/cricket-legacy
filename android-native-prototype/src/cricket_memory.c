#include "cricket_memory.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "CricketMemory"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

bool cricket_memory_init(CricketVirtualArena* arena, size_t arena_size) {
    if (!arena) return false;
    
    // Default to 4GB address space if 0 requested
    if (arena_size == 0) {
        arena_size = 0x100000000ULL; // 4 Gigabytes
    }

    // Allocate anonymous memory with MAP_NORESERVE
    // On Linux/Android, pages are not backed by physical RAM until touched
    void* ptr = mmap(NULL, arena_size, PROT_READ | PROT_WRITE, 
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);

    if (ptr == MAP_FAILED) {
        LOGE("Failed to allocate %zu bytes virtual arena! Errno: %d", arena_size, errno);
        arena->base_address = NULL;
        arena->reserved_size = 0;
        arena->is_initialized = false;
        return false;
    }

    arena->base_address = ptr;
    arena->reserved_size = arena_size;
    arena->committed_size = 0;
    arena->is_initialized = true;

    LOGI("Successfully allocated 4GB virtual arena at base: %p", ptr);
    return true;
}

uint32_t cricket_read_u32(CricketVirtualArena* arena, uint32_t x86_addr) {
    if (!arena || !arena->is_initialized) return 0;
    
    uint8_t* target = (uint8_t*)arena->base_address + x86_addr;
    uint32_t value;
    // memcpy avoids ARM64 hardware unaligned memory faults
    memcpy(&value, target, sizeof(uint32_t));
    return value;
}

void cricket_write_u32(CricketVirtualArena* arena, uint32_t x86_addr, uint32_t value) {
    if (!arena || !arena->is_initialized) return;

    uint8_t* target = (uint8_t*)arena->base_address + x86_addr;
    // memcpy avoids ARM64 hardware unaligned memory faults
    memcpy(target, &value, sizeof(uint32_t));
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
    if (arena && arena->base_address) {
        munmap(arena->base_address, arena->reserved_size);
        LOGI("Released virtual memory arena at %p", arena->base_address);
        arena->base_address = NULL;
        arena->reserved_size = 0;
        arena->is_initialized = false;
    }
}
