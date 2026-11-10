#include "game_static_data.h"
#include "cricket_memory.h"
#include "upstream_d3d8/cpu.h"
#include "upstream_d3d8/win32.h"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>

#define TAG "CricketLegacy-StaticData"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Upstream global bounds variables defined in runtime.h / upstream_core.c
extern uint32_t g_image_lo, g_image_hi;
extern uint32_t g_state_lo, g_state_hi;
extern uint8_t *g_mem;
extern uint32_t g_device_object;

static CricketPEImageState s_pe_image = {0};

bool game_static_data_init(void) {
    // 1. Ensure the 4GB Cricket Virtual Memory Arena is active
    CricketVirtualArena *arena = cricket_memory_get_arena();
    if (!arena || !arena->is_initialized) {
        LOGI("Initializing Cricket Virtual Memory Arena for Static Data...");
        if (!cricket_memory_init(NULL, 0)) {
            LOGE("Failed to initialize Cricket Virtual Memory Arena!");
            return false;
        }
        arena = cricket_memory_get_arena();
    }
    g_mem = (uint8_t*)arena->base_address;

    LOGI("==================================================================");
    LOGI("PHASE C.2 STAGE 1: PREPARING GAME STATIC DATA & ORIGINAL MEMORY");
    LOGI("==================================================================");

    // 2. Define the authentic PE Section Table for Cricket07.exe
    s_pe_image.image_base = CRICKET_IMAGE_BASE;
    s_pe_image.entry_point = CRICKET_TEXT_START; // Entry point RVA 0x1000 -> 0x00401000
    s_pe_image.image_size = CRICKET_IMAGE_SIZE;
    s_pe_image.num_sections = 4;

    // Section 0: .text (Code)
    strncpy(s_pe_image.sections[0].name, ".text", 8);
    s_pe_image.sections[0].va = CRICKET_TEXT_START;
    s_pe_image.sections[0].size = CRICKET_TEXT_SIZE;
    s_pe_image.sections[0].flags = PE_SCN_CNT_CODE | PE_SCN_MEM_EXECUTE | PE_SCN_MEM_READ;
    s_pe_image.sections[0].is_writable = false;

    // Section 1: .rdata (Read-only data, string constants, RTTI)
    strncpy(s_pe_image.sections[1].name, ".rdata", 8);
    s_pe_image.sections[1].va = CRICKET_RDATA_START;
    s_pe_image.sections[1].size = CRICKET_RDATA_SIZE;
    s_pe_image.sections[1].flags = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ;
    s_pe_image.sections[1].is_writable = false;

    // Section 2: .data (Initialized read-write global variables)
    strncpy(s_pe_image.sections[2].name, ".data", 8);
    s_pe_image.sections[2].va = CRICKET_DATA_START;
    s_pe_image.sections[2].size = CRICKET_DATA_SIZE;
    s_pe_image.sections[2].flags = PE_SCN_CNT_INITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
    s_pe_image.sections[2].is_writable = true;

    // Section 3: .bss (Uninitialized read-write global variables)
    strncpy(s_pe_image.sections[3].name, ".bss", 8);
    s_pe_image.sections[3].va = CRICKET_BSS_START;
    s_pe_image.sections[3].size = CRICKET_BSS_SIZE;
    s_pe_image.sections[3].flags = PE_SCN_CNT_UNINITIALIZED_DATA | PE_SCN_MEM_READ | PE_SCN_MEM_WRITE;
    s_pe_image.sections[3].is_writable = true;

    s_pe_image.state_lo = CRICKET_DATA_START;
    s_pe_image.state_hi = CRICKET_IMAGE_END;

    // 3. Set upstream engine global boundary markers
    g_image_lo = CRICKET_IMAGE_BASE;
    g_image_hi = CRICKET_IMAGE_END;
    g_state_lo = CRICKET_DATA_START;
    g_state_hi = CRICKET_IMAGE_END;

    // 4. Initialize Synthetic PE DOS and NT Headers at ImageBase (0x00400000)
    uint8_t *img_base = (uint8_t*)HP(CRICKET_IMAGE_BASE);
    memset(img_base, 0, 0x1000); // 4KB header space

    // DOS Header: "MZ" signature at offset 0, e_lfanew at 0x3C pointing to NT headers (0x80)
    img_base[0] = 'M';
    img_base[1] = 'Z';
    W32(CRICKET_IMAGE_BASE + 0x3C, 0x00000080u);

    // NT Header: "PE\0\0" at offset 0x80
    uint8_t *nt_hdr = (uint8_t*)HP(CRICKET_IMAGE_BASE + 0x80);
    nt_hdr[0] = 'P';
    nt_hdr[1] = 'E';
    nt_hdr[2] = 0;
    nt_hdr[3] = 0;

    // File Header: Machine = 0x014c (i386), NumberOfSections = 4
    W16(CRICKET_IMAGE_BASE + 0x84, 0x014Cu);
    W16(CRICKET_IMAGE_BASE + 0x86, 4);

    // Optional Header: Magic = 0x010B (PE32), ImageBase = 0x00400000, SizeOfImage = 0x00700000
    W16(CRICKET_IMAGE_BASE + 0x98, 0x010Bu);
    W32(CRICKET_IMAGE_BASE + 0xB4, CRICKET_IMAGE_BASE);
    W32(CRICKET_IMAGE_BASE + 0xD0, CRICKET_IMAGE_SIZE);

    LOGI("Synthesized authentic PE headers at guest address 0x%08x", CRICKET_IMAGE_BASE);

    // 5. Zero-initialize the entire .bss section
    LOGI("Zero-initializing .bss section: [0x%08x - 0x%08x] (%u KB)...",
         CRICKET_BSS_START, CRICKET_BSS_START + CRICKET_BSS_SIZE, CRICKET_BSS_SIZE / 1024);
    memset(HP(CRICKET_BSS_START), 0, CRICKET_BSS_SIZE);

    // 6. Set up Guest Thread Information Block (GUEST_TIB at 0x00010000)
    LOGI("Initializing Win32 Thread Information Block (TIB) at 0x%08x...", CRICKET_GUEST_TIB);
    W32(CRICKET_GUEST_TIB + 0x00, 0xFFFFFFFFu);                                     // Empty SEH chain
    W32(CRICKET_GUEST_TIB + 0x04, CRICKET_GUEST_STACK_TOP);                         // Stack Base
    W32(CRICKET_GUEST_TIB + 0x08, CRICKET_GUEST_STACK_TOP - CRICKET_GUEST_STACK_SIZE); // Stack Limit
    W32(CRICKET_GUEST_TIB + 0x18, CRICKET_GUEST_TIB);                              // Self pointer
    W32(CRICKET_GUEST_TIB + 0x20, 0x00001000u);                                    // Process ID
    W32(CRICKET_GUEST_TIB + 0x24, 0x00001001u);                                    // Thread ID

    // 7. Initialize known Cricket 07 baseline global variables in .data and .bss
    // Menu Cursor position: center of 640x480 screen (x = 320.0f, y = 240.0f)
    float cursor_x = 320.0f;
    float cursor_y = 240.0f;
    uint32_t cx_bits, cy_bits;
    memcpy(&cx_bits, &cursor_x, 4);
    memcpy(&cy_bits, &cursor_y, 4);
    W32(CRICKET_VAR_CURSOR_POS + 0, cx_bits);
    W32(CRICKET_VAR_CURSOR_POS + 4, cy_bits);
    LOGI("Initialized PROFILE_CURSOR at 0x%08x: (%.1f, %.1f)", CRICKET_VAR_CURSOR_POS, cursor_x, cursor_y);

    // Game state: 0 (Uninitialized / Pre-Init)
    W32(CRICKET_VAR_GAME_STATE, 0);

    // Widescreen state: 1 (Hor+ Widescreen active)
    W32(CRICKET_VAR_WIDESCREEN, 1);

    // Audio muted flag: 0 (Audio enabled)
    W32(CRICKET_VAR_AUDIO_MUTED, 0);

    // Global D3D8 Device Pointer in static .data segment
    if (g_device_object) {
        W32(CRICKET_VAR_D3D_DEVICE, g_device_object);
        LOGI("Linked static D3D device variable at 0x%08x -> 0x%08x", CRICKET_VAR_D3D_DEVICE, g_device_object);
    }

    s_pe_image.is_initialized = true;
    LOGI("Game static data and memory layout successfully initialized.");
    game_static_data_dump_sections();
    return true;
}

const CricketPEImageState* game_static_data_get_state(void) {
    return &s_pe_image;
}

bool game_static_data_is_addr_in_section(uint32_t guest_addr, const char *sec_name) {
    if (!s_pe_image.is_initialized || !sec_name) return false;
    for (uint32_t i = 0; i < s_pe_image.num_sections; i++) {
        if (strcmp(s_pe_image.sections[i].name, sec_name) == 0) {
            uint32_t start = s_pe_image.sections[i].va;
            uint32_t end = start + s_pe_image.sections[i].size;
            return (guest_addr >= start && guest_addr < end);
        }
    }
    return false;
}

void game_static_data_dump_sections(void) {
    LOGI("------------------------------------------------------------------");
    LOGI("CRICKET 07 PE MEMORY LAYOUT SPECIFICATION:");
    LOGI("Image Base: 0x%08x | Image Size: 0x%08x (%u KB)",
         s_pe_image.image_base, s_pe_image.image_size, s_pe_image.image_size / 1024);
    for (uint32_t i = 0; i < s_pe_image.num_sections; i++) {
        const CricketPESection *s = &s_pe_image.sections[i];
        LOGI("  Section [%-6s]: 0x%08x - 0x%08x (Size: %6u KB) | Flags: 0x%08x [%s]",
             s->name, s->va, s->va + s->size, s->size / 1024, s->flags,
             s->is_writable ? "READ/WRITE" : "READ-ONLY");
    }
    LOGI("State Bounds: [0x%08x - 0x%08x] (Writable Data: %u KB)",
         s_pe_image.state_lo, s_pe_image.state_hi, (s_pe_image.state_hi - s_pe_image.state_lo) / 1024);
    LOGI("TIB: 0x%08x | Stack Top: 0x%08x | Heap Base: 0x%08x",
         CRICKET_GUEST_TIB, CRICKET_GUEST_STACK_TOP, CRICKET_GUEST_HEAP_BASE);
    LOGI("------------------------------------------------------------------");
}
