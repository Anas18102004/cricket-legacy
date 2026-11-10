#include "d3d8_android_adapter.h"
#include "render.h"
#include "cpu.h"
#include "win32.h"
#include "com.h"
#include "runtime.h"
#include "../cricket_memory.h"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>

#define TAG "CricketLegacy-D3D8"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Upstream globals
extern uint32_t g_device_object;
extern Device *g_device;
extern RenderBackend g_render_gpu;
extern RenderBackend *g_render;
extern ANativeWindow *g_android_native_window;
extern int g_window_w, g_window_h;
extern int g_screen_w, g_screen_h;
extern uint8_t *g_mem;

extern size_t g_gpu_tex_bytes;
extern uint32_t g_gpu_pipelines_made;

// Upstream methods in d3d8.c and render_wgpu.c
void w_Direct3DCreate8(CPU *c);
void d3d_CreateDevice(CPU *c);
void dev_CreateVertexBuffer(CPU *c);
void dev_CreateIndexBuffer(CPU *c);
void dev_CreateTexture(CPU *c);
void buf_Lock(CPU *c);
void buf_Unlock(CPU *c);
void tx_LockRect(CPU *c);
void tx_UnlockRect(CPU *c);
void dev_SetTransform(CPU *c);
void dev_SetViewport(CPU *c);
void dev_SetMaterial(CPU *c);
void dev_SetRenderState(CPU *c);
void dev_SetTexture(CPU *c);
void dev_SetTextureStageState(CPU *c);
void dev_SetStreamSource(CPU *c);
void dev_SetIndices(CPU *c);
void dev_SetVertexShader(CPU *c);
void dev_Clear(CPU *c);
void dev_BeginScene(CPU *c);
void dev_DrawPrimitive(CPU *c);
void dev_DrawIndexedPrimitive(CPU *c);
void dev_EndScene(CPU *c);
void dev_Present(CPU *c);
void configure_surface(void);
void test_d3d8_scene_render_frame(void);

// Safe scratch memory addresses within the guest arena (below 1MB guest heap)
#define SCRATCH_BASE        0x000E0000u
#define SCRATCH_STACK       (SCRATCH_BASE + 0x1000u)
#define SCRATCH_TEMP_OUT    (SCRATCH_BASE + 0x2000u)
#define SCRATCH_MATRIX      (SCRATCH_BASE + 0x2100u)
#define SCRATCH_VIEWPORT    (SCRATCH_BASE + 0x2200u)
#define SCRATCH_MATERIAL    (SCRATCH_BASE + 0x2300u)
#define D3D_PP_ADDR         (SCRATCH_BASE + 0x2400u)
#define D3D_OUT_DEV_ADDR    (SCRATCH_BASE + 0x2500u)

static D3D8AndroidState s_adapter_state = {0};
static uint32_t s_d3d_object = 0;

bool d3d8_android_init(ANativeWindow* window) {
    if (!window) {
        LOGE("d3d8_android_init: NULL ANativeWindow provided");
        return false;
    }

    // 1. Ensure Cricket Memory Arena is initialized
    CricketVirtualArena *arena = cricket_memory_get_arena();
    if (!arena || !arena->is_initialized) {
        LOGI("Initializing cricket memory arena...");
        if (!cricket_memory_init(NULL, 0)) {
            LOGE("Failed to initialize cricket memory arena");
            return false;
        }
        arena = cricket_memory_get_arena();
    }
    g_mem = (uint8_t*)arena->base_address;

    // 2. Bind ANativeWindow to upstream render_wgpu
    g_android_native_window = window;
    int32_t width = ANativeWindow_getWidth(window);
    int32_t height = ANativeWindow_getHeight(window);
    if (width <= 0 || height <= 0) {
        width = 1280;
        height = 720;
    }
    g_window_w = width;
    g_window_h = height;
    g_screen_w = width;
    g_screen_h = height;
    LOGI("d3d8_android_init: Window surface size %dx%d", width, height);

    // 3. Set Direct3D render backend to WebGPU recorder
    g_render = &g_render_gpu;

    // 4. Create IDirect3D8 instance
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);       // Return address
    W32(c.esp + 4, 220);     // Direct3D SDK version 220
    w_Direct3DCreate8(&c);
    s_d3d_object = c.eax;
    if (!s_d3d_object) {
        LOGE("Failed to create IDirect3D8 object");
        return false;
    }
    LOGI("Created IDirect3D8 object: 0x%08x", s_d3d_object);

    // 5. Fill D3DPRESENT_PARAMETERS in guest memory
    memset(HP(D3D_PP_ADDR), 0, 64);
    W32(D3D_PP_ADDR + 0, (uint32_t)width);          // BackBufferWidth
    W32(D3D_PP_ADDR + 4, (uint32_t)height);         // BackBufferHeight
    W32(D3D_PP_ADDR + 8, FMT_X8R8G8B8);             // BackBufferFormat
    W32(D3D_PP_ADDR + 12, 1);                       // BackBufferCount
    W32(D3D_PP_ADDR + 16, 0);                       // MultiSampleType
    W32(D3D_PP_ADDR + 20, 1);                       // SwapEffect (DISCARD)
    W32(D3D_PP_ADDR + 24, MAIN_HWND);               // hDeviceWindow
    W32(D3D_PP_ADDR + 28, 1);                       // Windowed
    W32(D3D_PP_ADDR + 32, 1);                       // EnableAutoDepthStencil
    W32(D3D_PP_ADDR + 36, FMT_D24S8);               // AutoDepthStencilFormat
    W32(D3D_PP_ADDR + 40, 0);                       // Flags

    // 6. Invoke IDirect3D8::CreateDevice
    memset(&c, 0, sizeof(c));
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_d3d_object);                   // THIS
    W32(c.esp + 8, 0);                              // Adapter (DEFAULT)
    W32(c.esp + 12, 1);                             // DeviceType (HAL = 1)
    W32(c.esp + 16, MAIN_HWND);                      // FocusWindow
    W32(c.esp + 20, 0x40);                          // Behavior (HARDWARE_VERTEXPROCESSING)
    W32(c.esp + 24, D3D_PP_ADDR);                   // PresentationParameters
    W32(c.esp + 28, D3D_OUT_DEV_ADDR);              // ppReturnedDeviceInterface
    d3d_CreateDevice(&c);

    s_adapter_state.guest_device_obj = R32(D3D_OUT_DEV_ADDR);
    s_adapter_state.device = (Device*)com_data(s_adapter_state.guest_device_obj);
    if (!s_adapter_state.device) {
        LOGE("Failed to create Direct3D 8 Device!");
        return false;
    }

    s_adapter_state.is_initialized = true;
    s_adapter_state.width = (uint32_t)width;
    s_adapter_state.height = (uint32_t)height;
    s_adapter_state.frame_count = 0;
    s_adapter_state.total_draws = 0;
    s_adapter_state.gpu_tex_bytes = 0;
    s_adapter_state.pipelines_made = 0;

    LOGI("Direct3D 8 Android Adapter successfully initialized: Device %p (Guest 0x%08x)",
         s_adapter_state.device, s_adapter_state.guest_device_obj);
    return true;
}

void d3d8_android_resize(uint32_t width, uint32_t height) {
    if (!s_adapter_state.is_initialized || width == 0 || height == 0) return;
    g_window_w = (int)width;
    g_window_h = (int)height;
    s_adapter_state.width = width;
    s_adapter_state.height = height;
    if (s_adapter_state.device) {
        s_adapter_state.device->width = width;
        s_adapter_state.device->height = height;
    }
    configure_surface();
    LOGI("d3d8_android_resize: Swapchain reconfigured to %ux%u", width, height);
}

bool d3d8_android_render_frame(void) {
    if (!s_adapter_state.is_initialized) return false;
    test_d3d8_scene_render_frame();
    if (s_adapter_state.device) {
        s_adapter_state.frame_count = s_adapter_state.device->frame;
        s_adapter_state.gpu_tex_bytes = g_gpu_tex_bytes;
        s_adapter_state.pipelines_made = g_gpu_pipelines_made;
    }
    return true;
}

void d3d8_android_shutdown(void) {
    if (!s_adapter_state.is_initialized) return;
    LOGI("Shutting down D3D8 Android Adapter...");
    s_adapter_state.is_initialized = false;
    s_adapter_state.device = NULL;
    s_adapter_state.guest_device_obj = 0;
}

const D3D8AndroidState* d3d8_android_get_state(void) {
    if (s_adapter_state.is_initialized && s_adapter_state.device) {
        s_adapter_state.frame_count = s_adapter_state.device->frame;
        s_adapter_state.gpu_tex_bytes = g_gpu_tex_bytes;
        s_adapter_state.pipelines_made = g_gpu_pipelines_made;
    }
    return &s_adapter_state;
}

Device* d3d8_android_get_device(void) {
    return s_adapter_state.device;
}

bool d3d8_android_verify_com_dispatch(void) {
    if (!s_adapter_state.guest_device_obj) {
        LOGE("verify_com_dispatch: guest_device_obj is NULL");
        return false;
    }
    uint32_t g_dev = s_adapter_state.guest_device_obj;
    ComObj *o = com_get(g_dev);
    if (!o) {
        LOGE("verify_com_dispatch: com_get(0x%08x) failed", g_dev);
        return false;
    }
    LOGI("verify_com_dispatch: ComObj at 0x%08x, type %d, iface %s", g_dev, o->type, o->iface->name);

    // 1. Read vtable pointer in guest memory
    uint32_t vt = R32(g_dev);
    LOGI("verify_com_dispatch: guest vtable at 0x%08x", vt);
    if (!vt) {
        LOGE("verify_com_dispatch: vtable is NULL");
        return false;
    }

    // 2. Check Slot 34: BeginScene (nargs=0)
    uint32_t magic_begin = R32(vt + 34 * 4);
    const char *name_begin = import_name(magic_begin);
    LOGI("verify_com_dispatch: Slot 34 (BeginScene) -> magic 0x%08x (%s)",
         magic_begin, name_begin ? name_begin : "?");

    // 3. Check Slot 71: DrawIndexedPrimitive (nargs=5)
    uint32_t magic_draw = R32(vt + 71 * 4);
    const char *name_draw = import_name(magic_draw);
    LOGI("verify_com_dispatch: Slot 71 (DrawIndexedPrimitive) -> magic 0x%08x (%s)",
         magic_draw, name_draw ? name_draw : "?");

    // 4. Check Slot 35: EndScene (nargs=0)
    uint32_t magic_end = R32(vt + 35 * 4);
    const char *name_end = import_name(magic_end);
    LOGI("verify_com_dispatch: Slot 35 (EndScene) -> magic 0x%08x (%s)",
         magic_end, name_end ? name_end : "?");

    if (magic_begin < IMPORT_BASE || magic_draw < IMPORT_BASE || magic_end < IMPORT_BASE) {
        LOGE("verify_com_dispatch: vtable slots not in IMPORT_BASE range");
        return false;
    }

    // 5. Test dispatch through call_indirect
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0x00401000); // Return address
    W32(c.esp + 4, g_dev);      // THIS
    call_indirect(&c, magic_begin);
    if (c.eax != S_OK) {
        LOGE("verify_com_dispatch: call_indirect(BeginScene) returned 0x%08x", c.eax);
        return false;
    }
    LOGI("verify_com_dispatch: call_indirect(BeginScene) SUCCEEDED (eax=0x%08x)", c.eax);

    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0x00401000);
    W32(c.esp + 4, g_dev);
    call_indirect(&c, magic_end);
    if (c.eax != S_OK) {
        LOGE("verify_com_dispatch: call_indirect(EndScene) returned 0x%08x", c.eax);
        return false;
    }
    LOGI("verify_com_dispatch: call_indirect(EndScene) SUCCEEDED (eax=0x%08x)", c.eax);

    LOGI("verify_com_dispatch: COM dispatch verified end-to-end via call_indirect.");
    return true;
}

// ============================================================================
// Direct3D 8 C Wrappers
// ============================================================================

uint32_t d3d8_CreateVertexBuffer(Device *d, uint32_t len, uint32_t usage, uint32_t fvf, uint32_t pool) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, len);
    W32(c.esp + 12, usage);
    W32(c.esp + 16, fvf);
    W32(c.esp + 20, pool);
    W32(c.esp + 24, SCRATCH_TEMP_OUT);
    dev_CreateVertexBuffer(&c);
    return R32(SCRATCH_TEMP_OUT);
}

uint32_t d3d8_CreateIndexBuffer(Device *d, uint32_t len, uint32_t usage, uint32_t fmt, uint32_t pool) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, len);
    W32(c.esp + 12, usage);
    W32(c.esp + 16, fmt);
    W32(c.esp + 20, pool);
    W32(c.esp + 24, SCRATCH_TEMP_OUT);
    dev_CreateIndexBuffer(&c);
    return R32(SCRATCH_TEMP_OUT);
}

uint32_t d3d8_CreateTexture(Device *d, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, w);
    W32(c.esp + 12, h);
    W32(c.esp + 16, levels);
    W32(c.esp + 20, usage);
    W32(c.esp + 24, fmt);
    W32(c.esp + 28, pool);
    W32(c.esp + 32, SCRATCH_TEMP_OUT);
    dev_CreateTexture(&c);
    return R32(SCRATCH_TEMP_OUT);
}

void d3d8_BufferLock(uint32_t buf_handle, uint32_t offset, uint32_t size, void **out_ptr, uint32_t flags) {
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, buf_handle);
    W32(c.esp + 8, offset);
    W32(c.esp + 12, size);
    W32(c.esp + 16, SCRATCH_TEMP_OUT);
    W32(c.esp + 20, flags);
    buf_Lock(&c);
    uint32_t guest_addr = R32(SCRATCH_TEMP_OUT);
    if (out_ptr) *out_ptr = (void*)HP(guest_addr);
}

void d3d8_BufferUnlock(uint32_t buf_handle) {
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, buf_handle);
    buf_Unlock(&c);
}

void d3d8_TextureLockRect(uint32_t tex_handle, uint32_t level, void **out_ptr, uint32_t *out_pitch, uint32_t flags) {
    (void)flags;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, tex_handle);
    W32(c.esp + 8, level);
    W32(c.esp + 12, SCRATCH_TEMP_OUT);
    W32(c.esp + 16, 0);
    tx_LockRect(&c);
    if (out_pitch) *out_pitch = R32(SCRATCH_TEMP_OUT);
    uint32_t guest_addr = R32(SCRATCH_TEMP_OUT + 4);
    if (out_ptr) *out_ptr = (void*)HP(guest_addr);
}

void d3d8_TextureUnlockRect(uint32_t tex_handle, uint32_t level) {
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, tex_handle);
    W32(c.esp + 8, level);
    tx_UnlockRect(&c);
}

void d3d8_SetTransform(Device *d, uint32_t state, const float m[16]) {
    (void)d;
    memcpy(HP(SCRATCH_MATRIX), m, 64);
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, state);
    W32(c.esp + 12, SCRATCH_MATRIX);
    dev_SetTransform(&c);
}

void d3d8_SetViewport(Device *d, uint32_t x, uint32_t y, uint32_t w, uint32_t h, float minz, float maxz) {
    (void)d;
    W32(SCRATCH_VIEWPORT + 0, x);
    W32(SCRATCH_VIEWPORT + 4, y);
    W32(SCRATCH_VIEWPORT + 8, w);
    W32(SCRATCH_VIEWPORT + 12, h);
    WF32(SCRATCH_VIEWPORT + 16, minz);
    WF32(SCRATCH_VIEWPORT + 20, maxz);
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, SCRATCH_VIEWPORT);
    dev_SetViewport(&c);
}

void d3d8_SetMaterial(Device *d, const float mat[17]) {
    (void)d;
    memcpy(HP(SCRATCH_MATERIAL), mat, 68);
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, SCRATCH_MATERIAL);
    dev_SetMaterial(&c);
}

void d3d8_SetRenderState(Device *d, uint32_t state, uint32_t value) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, state);
    W32(c.esp + 12, value);
    dev_SetRenderState(&c);
}

void d3d8_SetTexture(Device *d, uint32_t stage, uint32_t tex_handle) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, stage);
    W32(c.esp + 12, tex_handle);
    dev_SetTexture(&c);
}

void d3d8_SetTextureStageState(Device *d, uint32_t stage, uint32_t state, uint32_t value) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, stage);
    W32(c.esp + 12, state);
    W32(c.esp + 16, value);
    dev_SetTextureStageState(&c);
}

void d3d8_SetStreamSource(Device *d, uint32_t stream, uint32_t vb_handle, uint32_t stride) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, stream);
    W32(c.esp + 12, vb_handle);
    W32(c.esp + 16, stride);
    dev_SetStreamSource(&c);
}

void d3d8_SetIndices(Device *d, uint32_t ib_handle, uint32_t base_vertex) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, ib_handle);
    W32(c.esp + 12, base_vertex);
    dev_SetIndices(&c);
}

void d3d8_SetVertexShader(Device *d, uint32_t handle_or_fvf) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, handle_or_fvf);
    dev_SetVertexShader(&c);
}

void d3d8_Clear(Device *d, uint32_t count, uint32_t flags, uint32_t color, float z, uint32_t stencil) {
    (void)d;
    uint32_t zbits;
    memcpy(&zbits, &z, 4);
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, count);
    W32(c.esp + 12, 0); // pRects
    W32(c.esp + 16, flags);
    W32(c.esp + 20, color);
    W32(c.esp + 24, zbits);
    W32(c.esp + 28, stencil);
    dev_Clear(&c);
}

void d3d8_BeginScene(Device *d) {
    (void)d;
    uint32_t vt = R32(s_adapter_state.guest_device_obj);
    uint32_t target = vt ? R32(vt + 34 * 4) : 0;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    if (target) call_indirect(&c, target);
    else dev_BeginScene(&c);
}

void d3d8_DrawPrimitive(Device *d, uint32_t prim, uint32_t start_vertex, uint32_t prim_count) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, prim);
    W32(c.esp + 12, start_vertex);
    W32(c.esp + 16, prim_count);
    dev_DrawPrimitive(&c);
    s_adapter_state.total_draws++;
}

void d3d8_DrawIndexedPrimitive(Device *d, uint32_t prim, uint32_t min_idx, uint32_t num_verts, uint32_t start_idx, uint32_t prim_count) {
    (void)d;
    uint32_t vt = R32(s_adapter_state.guest_device_obj);
    uint32_t target = vt ? R32(vt + 71 * 4) : 0;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, prim);
    W32(c.esp + 12, min_idx);
    W32(c.esp + 16, num_verts);
    W32(c.esp + 20, start_idx);
    W32(c.esp + 24, prim_count);
    if (target) call_indirect(&c, target);
    else dev_DrawIndexedPrimitive(&c);
    s_adapter_state.total_draws++;
}

void d3d8_EndScene(Device *d) {
    (void)d;
    uint32_t vt = R32(s_adapter_state.guest_device_obj);
    uint32_t target = vt ? R32(vt + 35 * 4) : 0;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    if (target) call_indirect(&c, target);
    else dev_EndScene(&c);
}

void d3d8_Present(Device *d) {
    (void)d;
    CPU c = {0};
    c.esp = SCRATCH_STACK;
    W32(c.esp + 0, 0);
    W32(c.esp + 4, s_adapter_state.guest_device_obj);
    W32(c.esp + 8, 0);
    W32(c.esp + 12, 0);
    W32(c.esp + 16, 0);
    W32(c.esp + 20, 0);
    dev_Present(&c);
}
