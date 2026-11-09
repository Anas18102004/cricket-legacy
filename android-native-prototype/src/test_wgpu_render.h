#ifndef TEST_WGPU_RENDER_H
#define TEST_WGPU_RENDER_H

#include <android/native_window.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_initialized;
    bool frame_rendered;
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    char adapter_name[128];
    char backend_info[64];
} CricketWgpuRenderState;

// Initializes wgpu-native runtime, creates surface from ANativeWindow, adapter, device, and queue.
bool cricket_wgpu_init(ANativeWindow* window);

// Renders an actual frame through wgpu-native:
// Surface texture -> CommandEncoder -> RenderPass (Cricket turf green RGB) -> QueueSubmit -> SurfacePresent
bool cricket_wgpu_render_frame(void);

// Handles surface resizing and swapchain reconfiguration
void cricket_wgpu_resize(uint32_t width, uint32_t height);

// Cleanly releases all WebGPU and native window resources
void cricket_wgpu_shutdown(void);

// Queries the current rendering state and telemetry
const CricketWgpuRenderState* cricket_wgpu_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_WGPU_RENDER_H
