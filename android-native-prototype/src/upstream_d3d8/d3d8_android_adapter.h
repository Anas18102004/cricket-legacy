#ifndef D3D8_ANDROID_ADAPTER_H
#define D3D8_ANDROID_ADAPTER_H

#include <android/native_window.h>
#include <stdbool.h>
#include <stdint.h>
#include "d3d8.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_initialized;
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t total_draws;
    size_t gpu_tex_bytes;
    uint32_t pipelines_made;
    Device *device;
    uint32_t guest_device_obj;
} D3D8AndroidState;

// Initializes Android ANativeWindow surface, registers D3D8 backend, and creates the D3D8 Device
bool d3d8_android_init(ANativeWindow* window);

// Handles Android surface resize and reconfigures swapchain
void d3d8_android_resize(uint32_t width, uint32_t height);

// Executes a frame through Direct3D 8
bool d3d8_android_render_frame(void);

// Cleanly shuts down D3D8 and releases WebGPU resources
void d3d8_android_shutdown(void);

// Queries active state and device pointer
const D3D8AndroidState* d3d8_android_get_state(void);
Device* d3d8_android_get_device(void);

// Verifies COM vtable dispatch through guest memory and call_indirect
bool d3d8_android_verify_com_dispatch(void);

// C Wrappers directly invoking the original Direct3D 8 implementation
uint32_t d3d8_CreateVertexBuffer(Device *d, uint32_t len, uint32_t usage, uint32_t fvf, uint32_t pool);
uint32_t d3d8_CreateIndexBuffer(Device *d, uint32_t len, uint32_t usage, uint32_t fmt, uint32_t pool);
uint32_t d3d8_CreateTexture(Device *d, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool);

void d3d8_BufferLock(uint32_t buf_handle, uint32_t offset, uint32_t size, void **out_ptr, uint32_t flags);
void d3d8_BufferUnlock(uint32_t buf_handle);

void d3d8_TextureLockRect(uint32_t tex_handle, uint32_t level, void **out_ptr, uint32_t *out_pitch, uint32_t flags);
void d3d8_TextureUnlockRect(uint32_t tex_handle, uint32_t level);

void d3d8_SetTransform(Device *d, uint32_t state, const float m[16]);
void d3d8_SetViewport(Device *d, uint32_t x, uint32_t y, uint32_t w, uint32_t h, float minz, float maxz);
void d3d8_SetMaterial(Device *d, const float mat[17]);
void d3d8_SetRenderState(Device *d, uint32_t state, uint32_t value);
void d3d8_SetTexture(Device *d, uint32_t stage, uint32_t tex_handle);
void d3d8_SetTextureStageState(Device *d, uint32_t stage, uint32_t state, uint32_t value);

void d3d8_SetStreamSource(Device *d, uint32_t stream, uint32_t vb_handle, uint32_t stride);
void d3d8_SetIndices(Device *d, uint32_t ib_handle, uint32_t base_vertex);
void d3d8_SetVertexShader(Device *d, uint32_t handle_or_fvf);

void d3d8_Clear(Device *d, uint32_t count, uint32_t flags, uint32_t color, float z, uint32_t stencil);
void d3d8_BeginScene(Device *d);
void d3d8_DrawPrimitive(Device *d, uint32_t prim, uint32_t start_vertex, uint32_t prim_count);
void d3d8_DrawIndexedPrimitive(Device *d, uint32_t prim, uint32_t min_idx, uint32_t num_verts, uint32_t start_idx, uint32_t prim_count);
void d3d8_EndScene(Device *d);
void d3d8_Present(Device *d);

#ifdef __cplusplus
}
#endif

#endif // D3D8_ANDROID_ADAPTER_H
