#ifndef GAME_LAZY_VIEWS_H
#define GAME_LAZY_VIEWS_H

#include <stdint.h>
#include <stdbool.h>
#include "upstream_d3d8/win32.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIEW_CHUNK 0x10000u      // 64 KB per demand-paged chunk
#define VIEW_MAX_CHUNKS 0x10000u // Up to 4 GB view bitmap (65536 chunks)
#define MAX_LAZY_VIEWS 16

typedef struct {
    pthread_mutex_t lock;
    uint32_t a;                  // Guest base address
    uint32_t len;                // Length in bytes
    int fd;                      // Host file descriptor
    uint8_t filled[VIEW_MAX_CHUNKS / 8]; // Bitmask of paged chunks
} LazyView;

// System lifecycle
void game_lazy_views_init(void);
void game_lazy_views_shutdown(void);

// Lazy View Management
uint32_t guest_view_map(int fd, uint32_t len, uint32_t eager);
int guest_view_unmap(uint32_t a);
uint32_t guest_view_end(uint32_t a);
void view_fill(uint32_t src, uint32_t n);
bool is_chunk_filled(uint32_t view_idx, uint32_t chunk_idx);

// EA Memcpy Entry Hook (Hook for 0x652a00)
void hook_copy_source(CPU *c);

#ifdef __cplusplus
}
#endif

#endif // GAME_LAZY_VIEWS_H
