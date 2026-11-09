#ifndef CRICKET_MEMORY_H
#define CRICKET_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Unaligned integer types that Clang/GCC on ARM64 can load/store with single instructions
typedef uint16_t __attribute__((aligned(1))) unaligned_u16;
typedef uint32_t __attribute__((aligned(1))) unaligned_u32;
typedef uint64_t __attribute__((aligned(1))) unaligned_u64;
typedef float    __attribute__((aligned(1))) unaligned_f32;

typedef enum {
    ARENA_MODE_UNINITIALIZED = 0,
    ARENA_MODE_FIXED_LOWER_4GB = 1, // Mapped directly in low 32-bit memory (raw absolute pointers valid)
    ARENA_MODE_BASE_OFFSET = 2      // Mapped at arbitrary 64-bit base (relative offsetting required)
} CricketArenaMode;

typedef struct {
    void* base_address;
    size_t reserved_size;
    CricketArenaMode mode;
    bool is_initialized;
} CricketVirtualArena;

// Initializes the 4GB virtual address arena.
// First attempts MAP_FIXED_NOREPLACE at low memory (0x00010000 - 0x100000000ULL).
// If collision occurs with Bionic/Android runtime, gracefully falls back to base-relative mapping.
bool cricket_memory_init(CricketVirtualArena* arena, size_t arena_size);

// Translates a 32-bit x86 address to a native 64-bit pointer with 32-bit wraparound masking
static inline void* cricket_translate_addr(const CricketVirtualArena* arena, uint32_t x86_addr) {
    // Mask ensures 32-bit wraparound semantics match original x86 registers
    uintptr_t offset = (uintptr_t)(x86_addr & 0xFFFFFFFFU);
    return (void*)((uint8_t*)arena->base_address + offset);
}

// Fast inline unaligned 32-bit read (avoids function call overhead in tight loops)
static inline uint32_t cricket_read_u32_fast(const CricketVirtualArena* arena, uint32_t x86_addr) {
    const unaligned_u32* ptr = (const unaligned_u32*)cricket_translate_addr(arena, x86_addr);
    return *ptr;
}

// Fast inline unaligned 32-bit write
static inline void cricket_write_u32_fast(CricketVirtualArena* arena, uint32_t x86_addr, uint32_t val) {
    unaligned_u32* ptr = (unaligned_u32*)cricket_translate_addr(arena, x86_addr);
    *ptr = val;
}

// Standard external API functions
uint32_t cricket_read_u32(CricketVirtualArena* arena, uint32_t x86_addr);
void cricket_write_u32(CricketVirtualArena* arena, uint32_t x86_addr, uint32_t value);
size_t cricket_get_resident_memory_kb(void);
void cricket_memory_cleanup(CricketVirtualArena* arena);

#ifdef __cplusplus
}
#endif

#endif // CRICKET_MEMORY_H
