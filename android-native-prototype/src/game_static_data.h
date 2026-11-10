#ifndef GAME_STATIC_DATA_H
#define GAME_STATIC_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Authentic Cricket 07 Win32 memory map constants
#define CRICKET_IMAGE_BASE       0x00400000u
#define CRICKET_TEXT_START       0x00401000u
#define CRICKET_TEXT_SIZE        0x003DF000u // 4,059,136 bytes (~3.87 MB)
#define CRICKET_RDATA_START      0x007E0000u
#define CRICKET_RDATA_SIZE       0x000C0000u // 786,432 bytes (~768 KB)
#define CRICKET_DATA_START       0x008A0000u
#define CRICKET_DATA_SIZE        0x00160000u // 1,441,792 bytes (~1.375 MB)
#define CRICKET_BSS_START        0x00A00000u
#define CRICKET_BSS_SIZE         0x00100000u // 1,048,576 bytes (1.0 MB)
#define CRICKET_IMAGE_SIZE       0x00700000u // 7,340,032 bytes (7.0 MB total PE image)
#define CRICKET_IMAGE_END        (CRICKET_IMAGE_BASE + CRICKET_IMAGE_SIZE) // 0x00B00000u

// Guest runtime regions
#define CRICKET_GUEST_TIB        0x00010000u
#define CRICKET_GUEST_STACK_TOP  0x00300000u
#define CRICKET_GUEST_STACK_SIZE 0x00200000u // 2MB Stack
#define CRICKET_GUEST_HEAP_BASE  0x10000000u
#define CRICKET_GUEST_HEAP_HI    0x60000000u // 1.25 GB Heap
#define CRICKET_GUEST_VALLOC_LO  0x60000000u
#define CRICKET_GUEST_VALLOC_HI  0xE0000000u // 2.0 GB VirtualAlloc arena
#define CRICKET_IMPORT_BASE      0xF0000000u

// Known Cricket 07 Global Variables (from profiles/cricket07.h & Ghidra symbols)
#define CRICKET_VAR_CURSOR_POS   0x00A01970u // float x, float y in 640x480 (menu cursor)
#define CRICKET_VAR_GAME_STATE   0x008A1000u // uint32_t: 0=Init, 1=Menu, 2=Match
#define CRICKET_VAR_D3D_DEVICE   0x008A1010u // uint32_t: guest pointer to IDirect3DDevice8
#define CRICKET_VAR_WIDESCREEN   0x008A1020u // uint32_t: 1=widescreen Hor+ active
#define CRICKET_VAR_AUDIO_MUTED  0x008A1024u // uint32_t: 0=audible, 1=muted

// PE Section Characteristics
#define PE_SCN_CNT_CODE               0x00000020u
#define PE_SCN_CNT_INITIALIZED_DATA   0x00000040u
#define PE_SCN_CNT_UNINITIALIZED_DATA 0x00000080u
#define PE_SCN_MEM_EXECUTE            0x20000000u
#define PE_SCN_MEM_READ               0x40000000u
#define PE_SCN_MEM_WRITE              0x80000000u

typedef struct {
    char name[8];
    uint32_t va;          // Virtual Address (absolute guest address)
    uint32_t size;        // Virtual Size
    uint32_t flags;       // Characteristics
    bool is_writable;
} CricketPESection;

typedef struct {
    bool is_initialized;
    uint32_t image_base;
    uint32_t entry_point;
    uint32_t image_size;
    uint32_t num_sections;
    CricketPESection sections[4]; // .text, .rdata, .data, .bss
    uint32_t state_lo;            // Start of writable memory (.data)
    uint32_t state_hi;            // End of writable memory (.bss)
} CricketPEImageState;

// Initializes the game's static data segments within the 4GB virtual arena
bool game_static_data_init(void);

// Queries the initialized PE image state
const CricketPEImageState* game_static_data_get_state(void);

// Validates whether a guest address belongs to a specific section
bool game_static_data_is_addr_in_section(uint32_t guest_addr, const char *sec_name);

// Logs section memory bounds to logcat
void game_static_data_dump_sections(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_STATIC_DATA_H