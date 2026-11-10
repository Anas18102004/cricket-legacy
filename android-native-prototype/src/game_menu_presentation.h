#ifndef GAME_MENU_PRESENTATION_H
#define GAME_MENU_PRESENTATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_VIRTUAL_WIDTH   640.0f
#define MENU_VIRTUAL_HEIGHT  480.0f
#define MENU_NUM_BUTTONS     4

// Menu button identifiers
enum {
    MENU_BTN_EXHIBITION   = 0,
    MENU_BTN_TOURNAMENT   = 1,
    MENU_BTN_SETTINGS     = 2,
    MENU_BTN_DIAGNOSTICS  = 3
};

typedef struct {
    const char *label;
    float x1, y1, x2, y2; // Virtual 640x480 coordinate bounds
    bool hovered;
    bool pressed;
    uint32_t click_count;
} MenuButton;

typedef struct {
    bool is_initialized;
    float cursor_x, cursor_y;    // Current cursor coordinates in virtual 640x480
    float screen_w, screen_h;    // Native Android screen dimensions
    float scale;                 // 4:3 aspect ratio uniform scale factor
    float offset_x, offset_y;    // Pillarbox / letterbox margins in screen pixels
    int last_clicked_button;     // Last clicked button index (-1 if none)
    uint32_t total_clicks;       // Cumulative user button activations
    uint32_t rendered_frames;    // Frames rendered through D3D8 gateway
    MenuButton buttons[MENU_NUM_BUTTONS];
} MenuPresentationState;

// Subsystem Lifecycle
bool game_menu_init(void);
void game_menu_render_frame(void);
void game_menu_shutdown(void);

// Touchscreen Input & Cursor Steering
void game_menu_on_touch(int action, float screen_x, float screen_y);
void game_menu_screen_to_virtual(float sx, float sy, float *vx, float *vy);
void game_menu_virtual_to_screen(float vx, float vy, float *sx, float *sy);
void game_menu_get_cursor(float *cx, float *cy);
void game_menu_set_cursor(float cx, float cy);

// State Inspection
const MenuPresentationState* game_menu_get_state(void);
uint32_t game_menu_get_frame_count(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_MENU_PRESENTATION_H
