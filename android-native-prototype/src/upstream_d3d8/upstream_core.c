#include "win32.h"
#include "d3d8.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DYN_IMPORTS 8192

uint8_t *g_mem = NULL;
int g_screen_w = 1280;
int g_screen_h = 720;
int g_window_w = 1280;
int g_window_h = 720;

uint32_t g_state_lo = 0;
uint32_t g_state_hi = 0;
uint32_t g_det_sleeps = 0;
uint32_t g_det_waits = 0;
uint32_t g_det_reads = 0;

double guest_peek_seconds(void) { return host_seconds(); }
void guest_mem_ranges(uint32_t r[4]) { r[0] = r[1] = r[2] = r[3] = 0; }
uint32_t guest_view_end(uint32_t a) { (void)a; return 0; }
int refresh_hz(void) { return 60; }
void watch_arm(uint32_t addr) { (void)addr; }

typedef struct {
    char name[128];
    GuestFn fn;
} DynImportSlot;

static DynImportSlot g_dyn_imports[MAX_DYN_IMPORTS];
static uint32_t g_num_dyn_imports = 0;
__thread uint32_t g_import_target = 0;

// g_palettes defined in d3d8.c

#define HEAP_LO GUEST_HEAP_BASE
#define HEAP_HI GUEST_HEAP_HI
#define HEAP_MAGIC 0x4EA911C8u
#define NCLASS 256

static pthread_mutex_t g_heap_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_heap_top = HEAP_LO;
static uint32_t g_free[NCLASS + 1];

static uint32_t round16(uint32_t n) { return (n + 15) & ~15u; }

uint32_t heap_alloc(uint32_t size, int zero) {
    uint32_t sz = round16(size ? size : 1), a = 0;
    pthread_mutex_lock(&g_heap_lock);
    uint32_t cls = sz / 16;
    if (cls <= NCLASS && g_free[cls]) {
        a = g_free[cls];
        g_free[cls] = R32(a);
    } else if (cls > NCLASS) {
        uint32_t prev = 0, p = g_free[0];
        while (p && R32(p - 16) < sz) { prev = p; p = R32(p); }
        if (p) {
            a = p;
            if (prev) W32(prev, R32(p)); else g_free[0] = R32(p);
            sz = R32(p - 16);
        }
    }
    if (!a) {
        if (g_heap_top + 16 + sz > HEAP_HI) {
            pthread_mutex_unlock(&g_heap_lock);
            fprintf(stderr, "guest heap exhausted (%u bytes requested)\n", size);
            abort();
        }
        a = g_heap_top + 16;
        g_heap_top += 16 + sz;
        W32(a - 16, sz);
    }
    W32(a - 12, HEAP_MAGIC);
    pthread_mutex_unlock(&g_heap_lock);
    if (zero) memset(HP(a), 0, R32(a - 16));
    return a;
}

void heap_free(uint32_t a) {
    if (!a) return;
    if (a < HEAP_LO || a >= HEAP_HI || R32(a - 12) != HEAP_MAGIC) {
        return;
    }
    pthread_mutex_lock(&g_heap_lock);
    W32(a - 12, 0);
    uint32_t cls = R32(a - 16) / 16;
    if (cls > NCLASS) cls = 0;
    W32(a, g_free[cls]);
    g_free[cls] = a;
    pthread_mutex_unlock(&g_heap_lock);
}

uint32_t heap_size(uint32_t a) {
    return (a >= HEAP_LO && a < HEAP_HI && R32(a - 12) == HEAP_MAGIC) ? R32(a - 16) : 0;
}

uint32_t heap_realloc(uint32_t a, uint32_t size, int zero) {
    if (!a) return heap_alloc(size, zero);
    uint32_t old = heap_size(a);
    if (round16(size) <= old) return a;
    uint32_t b = heap_alloc(size, 0);
    memcpy(HP(b), HP(a), old);
    if (zero) memset(HP(b + old), 0, R32(b - 16) - old);
    heap_free(a);
    return b;
}

uint32_t add_dynamic_import(const char *name, GuestFn fn) {
    if (g_num_dyn_imports >= MAX_DYN_IMPORTS) {
        fprintf(stderr, "Dynamic imports limit reached!\n");
        abort();
    }
    uint32_t idx = g_num_dyn_imports++;
    snprintf(g_dyn_imports[idx].name, sizeof(g_dyn_imports[idx].name), "%s", name);
    g_dyn_imports[idx].fn = fn;
    return IMPORT_BASE + idx * 16;
}

const char *import_name(uint32_t addr) {
    if (addr < IMPORT_BASE || addr >= IMPORT_BASE + g_num_dyn_imports * 16) return NULL;
    return g_dyn_imports[(addr - IMPORT_BASE) / 16].name;
}

GuestFn find_import_handler(const char *name) {
    for (uint32_t i = 0; i < g_num_dyn_imports; i++) {
        if (strcmp(g_dyn_imports[i].name, name) == 0) return g_dyn_imports[i].fn;
    }
    return NULL;
}

void input_script_tick(uint32_t frame) {
    (void)frame;
}

void dump_tick(uint32_t frame) {
    (void)frame;
}

int widescreen_on(void) {
    return 1; // Default to widescreen for modern Android mobile aspect ratios
}

int settings_widescreen(void) {
    return 1;
}

void pace_frame(void) {
    // Android swapchain FIFO naturally waits for vsync
}

double host_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    static double start = -1.0;
    double now = (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    if (start < 0.0) start = now;
    return now - start + 60.0;
}

void guest_mem_stats(uint32_t *heap_used, uint32_t *valloc_used) {
    if (heap_used) *heap_used = g_heap_top - HEAP_LO;
    if (valloc_used) *valloc_used = 0;
}

void vclock_frame(void) {}

void netplay_frame(uint32_t frame, uint64_t (*state_hash)(void)) {
    (void)frame; (void)state_hash;
}

void settings_apply_to_game(uint32_t pp, const uint32_t regs[4]) {
    (void)pp; (void)regs;
}

void settings_poll(uint32_t frame) {
    (void)frame;
}

void widescreen_frame(void) {}

void guest_trap(CPU *c, uint32_t addr, const char *msg) {
    (void)c;
    fprintf(stderr, "[guest_trap] 0x%08x: %s\n", addr, msg);
    abort();
}
