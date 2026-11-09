#ifndef TEST_GRAPHICS_H
#define TEST_GRAPHICS_H

#include <stdbool.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_initialized;
    bool is_vulkan_supported;
    int surface_width;
    int surface_height;
    char renderer_info[128];
} CricketGraphicsState;

// Initializes graphics context on the provided native window (Vulkan primary, GLES fallback)
bool cricket_graphics_init(ANativeWindow* window);

// Handles window resize events
void cricket_graphics_resize(int width, int height);

// Renders a test frame (clears surface to stadium grass green)
void cricket_graphics_render_test_frame(void);

// Releases graphics resources
void cricket_graphics_shutdown(void);

// Queries current graphics state
const CricketGraphicsState* cricket_graphics_get_state(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_GRAPHICS_H
