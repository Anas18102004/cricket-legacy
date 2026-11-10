#include "game_lazy_views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <android/log.h>

#define LOG_TAG "CricketLazyViews"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static LazyView g_views[MAX_LAZY_VIEWS];
static int g_nviews = 0;
static pthread_mutex_t g_view_mgr_lock = PTHREAD_MUTEX_INITIALIZER;

#define VALLOC_LO GUEST_VALLOC_LO
#define VALLOC_HI GUEST_VALLOC_HI
static uint32_t g_valloc_top = VALLOC_LO;
static pthread_mutex_t g_valloc_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t guest_valloc(uint32_t size) {
    size = (size + 0xffff) & ~0xffffu; // 64KB granularity
    if (!size) size = 0x10000;

    pthread_mutex_lock(&g_valloc_lock);
    if (g_valloc_top + size > VALLOC_HI) {
        pthread_mutex_unlock(&g_valloc_lock);
        LOGE("guest_valloc: Virtual address space exhausted (%u bytes requested)", size);
        return 0;
    }
    uint32_t a = g_valloc_top;
    g_valloc_top += size;
    pthread_mutex_unlock(&g_valloc_lock);

    // Initialize allocated pages to zero
    memset(HP(a), 0, size);
    return a;
}

uint32_t guest_vfree(uint32_t a) {
    (void)a;
    // In mobile memory arena, simple high-water mark recycling is sufficient
    return 0;
}

void game_lazy_views_init(void) {
    pthread_mutex_lock(&g_view_mgr_lock);
    for (int i = 0; i < MAX_LAZY_VIEWS; i++) {
        pthread_mutex_init(&g_views[i].lock, NULL);
        g_views[i].a = 0;
        g_views[i].len = 0;
        g_views[i].fd = -1;
        memset(g_views[i].filled, 0, sizeof(g_views[i].filled));
    }
    g_nviews = 0;
    pthread_mutex_unlock(&g_view_mgr_lock);
    LOGI("Lazy Views Subsystem Initialized. Chunk size: %u bytes (64 KB)", VIEW_CHUNK);
}

void game_lazy_views_shutdown(void) {
    pthread_mutex_lock(&g_view_mgr_lock);
    for (int i = 0; i < MAX_LAZY_VIEWS; i++) {
        pthread_mutex_lock(&g_views[i].lock);
        g_views[i].a = 0;
        g_views[i].len = 0;
        g_views[i].fd = -1;
        memset(g_views[i].filled, 0, sizeof(g_views[i].filled));
        pthread_mutex_unlock(&g_views[i].lock);
        pthread_mutex_destroy(&g_views[i].lock);
    }
    g_nviews = 0;
    pthread_mutex_unlock(&g_view_mgr_lock);
    LOGI("Lazy Views Subsystem shut down.");
}

static int chunk_filled(LazyView *v, uint32_t k) {
    if ((k >> 3) >= sizeof(v->filled)) return 0;
    return __atomic_load_n(&v->filled[k >> 3], __ATOMIC_ACQUIRE) & (1u << (k & 7));
}

bool is_chunk_filled(uint32_t view_idx, uint32_t chunk_idx) {
    if (view_idx >= MAX_LAZY_VIEWS) return false;
    return chunk_filled(&g_views[view_idx], chunk_idx) != 0;
}

static void view_read_run(LazyView *v, uint32_t k0, uint32_t k1) {
    uint64_t run_end = (uint64_t)(k1 + 1) * VIEW_CHUNK;
    uint32_t at = k0 * VIEW_CHUNK;
    uint32_t end = (run_end < v->len) ? (uint32_t)run_end : v->len;
    uint32_t got = 0;

    while (at + got < end) {
        ssize_t r = pread(v->fd, HP(v->a + at + got), end - at - got, (off_t)at + got);
        if (r <= 0) break;
        got += (uint32_t)r;
    }

    for (uint32_t k = k0; k <= k1; k++) {
        uint64_t k_end = (uint64_t)(k + 1) * VIEW_CHUNK < v->len ? (uint64_t)(k + 1) * VIEW_CHUNK : v->len;
        if (k_end > (uint64_t)at + got) break;
        __atomic_or_fetch(&v->filled[k >> 3], (uint8_t)(1u << (k & 7)), __ATOMIC_RELEASE);
    }
}

uint32_t guest_view_map(int fd, uint32_t len, uint32_t eager) {
    if (fd < 0 || len == 0) return 0;

    pthread_mutex_lock(&g_view_mgr_lock);
    int slot = -1;
    for (int i = 0; i < MAX_LAZY_VIEWS; i++) {
        if (g_views[i].len == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        pthread_mutex_unlock(&g_view_mgr_lock);
        LOGE("guest_view_map: All lazy view slots in use (max %d)", MAX_LAZY_VIEWS);
        return 0;
    }

    uint32_t a = guest_valloc(len);
    if (!a) {
        pthread_mutex_unlock(&g_view_mgr_lock);
        return 0;
    }

    LazyView *v = &g_views[slot];
    pthread_mutex_lock(&v->lock);
    v->a = a;
    v->len = len;
    v->fd = fd;
    memset(v->filled, 0, sizeof(v->filled));

    if (slot >= g_nviews) {
        g_nviews = slot + 1;
    }

    // Eagerly pre-read initial directory / header pages
    if (eager > 0) {
        uint32_t k1 = (eager - 1) / VIEW_CHUNK;
        view_read_run(v, 0, k1);
    }

    pthread_mutex_unlock(&v->lock);
    pthread_mutex_unlock(&g_view_mgr_lock);

    LOGI("guest_view_map: Mapped view #%d at 0x%08X (Size: %u bytes, Eager: %u bytes)",
         slot, a, len, eager);
    return a;
}

int guest_view_unmap(uint32_t a) {
    if (!a) return 0;

    pthread_mutex_lock(&g_view_mgr_lock);
    for (int i = 0; i < g_nviews; i++) {
        LazyView *v = &g_views[i];
        if (v->a == a && v->len > 0) {
            pthread_mutex_lock(&v->lock);
            uint32_t len = v->len;
            v->a = 0;
            v->len = 0;
            v->fd = -1;
            memset(v->filled, 0, sizeof(v->filled));
            pthread_mutex_unlock(&v->lock);
            pthread_mutex_unlock(&g_view_mgr_lock);
            guest_vfree(a);
            LOGI("guest_view_unmap: Released view #%d at 0x%08X (%u bytes)", i, a, len);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_view_mgr_lock);
    return 0;
}

uint32_t guest_view_end(uint32_t a) {
    int nv = __atomic_load_n(&g_nviews, __ATOMIC_ACQUIRE);
    for (int i = 0; i < nv; i++) {
        LazyView *v = &g_views[i];
        uint32_t va = v->a;
        uint32_t len = __atomic_load_n(&v->len, __ATOMIC_ACQUIRE);
        if (len && a >= va && a < va + len) {
            return va + len;
        }
    }
    return 0;
}

void view_fill(uint32_t src, uint32_t n) {
    if (!n) return;
    int nv = __atomic_load_n(&g_nviews, __ATOMIC_ACQUIRE);
    for (int i = 0; i < nv; i++) {
        LazyView *v = &g_views[i];
        uint32_t len = __atomic_load_n(&v->len, __ATOMIC_ACQUIRE);
        uint32_t a = v->a;
        if (!len || src >= a + len || src + n <= a) continue;

        uint32_t lo = (src > a) ? (src - a) : 0;
        uint32_t hi = (src + n - a < len) ? (src + n - a) : len;
        uint32_t k0 = lo / VIEW_CHUNK;
        uint32_t k1 = (hi - 1) / VIEW_CHUNK;

        for (uint32_t k = k0; k <= k1; k++) {
            if (chunk_filled(v, k)) continue;
            pthread_mutex_lock(&v->lock);
            if (!chunk_filled(v, k)) {
                uint32_t run_end_k = k;
                while (run_end_k + 1 <= k1 && !chunk_filled(v, run_end_k + 1)) {
                    run_end_k++;
                }
                view_read_run(v, k, run_end_k);
                k = run_end_k;
            }
            pthread_mutex_unlock(&v->lock);
        }
    }
}

void hook_copy_source(CPU *c) {
    // In EA Sports x86 engine, plain memcpy arguments are at [esp+4] = dst, [esp+8] = src, [esp+12] = count
    uint32_t src = R32(c->esp + 8);
    uint32_t count = R32(c->esp + 12);
    view_fill(src, count);
}
