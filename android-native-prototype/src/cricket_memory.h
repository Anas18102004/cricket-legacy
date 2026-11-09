#ifndef CRICKET_MEMORY_H
#define CRICKET_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* base_address;
    size_t reserved_size;
    size_t committed_size;
    bool is_initialized;
} CricketVirtualArena;

// Initializes the contiguous 4GB virtual address space for 32-bit pointer mapping
bool cricket_memory_init(CricketVirtualArena* arena, size_t arena_size);

// Maps an emulated x86 32-bit address into a valid 64-bit native pointer
static inline void* cricket_translate_addr(CricketVirtualArena* arena, uint32_t x86_addr) {
    return (void*)((uint8_t*)arena->base_address + x86_addr);
}

// Safe unaligned 32-bit memory read for ARM64 architecture
uint32_t cricket_read_u32(CricketVirtualArena* arena, uint32_t x86_addr);

// Safe unaligned 32-bit memory write for ARM64 architecture
void cricket_write_u32(CricketVirtualArena* arena, uint32_t x86_addr, uint32_t value);

// Queries current resident set size (RSS) in kilobytes from /proc/self/statm
size_t cricket_get_resident_memory_kb(void);

// Releases virtual memory mapping
void cricket_memory_cleanup(CricketVirtualArena* arena);

#ifdef __cplusplus
}
#endif

#endif // CRICKET_MEMORY_H
