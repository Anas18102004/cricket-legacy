#ifndef CRICKET_MOBILE_INPUT_H
#define CRICKET_MOBILE_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum concurrent touch pointers
#define MOBILE_INPUT_MAX_POINTERS 10

// Batting control modes
typedef enum {
    BATTING_MODE_PURE_SWIPE = 0,        // Mode A: Swipe gesture only + automatic footwork
    BATTING_MODE_CLASSIC_JOYSTICK = 1,   // Mode B: Left thumbstick for direction + Right action buttons
    BATTING_MODE_PRO_JOYSTICK = 2,       // Mode C: Left thumbstick for crease shuffle + 360° Shot Map
    BATTING_MODE_HYBRID = 3              // Mode D: Left thumbstick + Swipe strokes + Contextual buttons
} BattingControlMode;

// Bowling control modes
typedef enum {
    BOWLING_MODE_GESTURE_AIM = 0,       // Mode A: Touch and drag pitch marker + tap/swipe release
    BOWLING_MODE_CLASSIC_RETICLE = 1     // Mode B: D-Pad / Thumbstick aiming + Button release
} BowlingControlMode;

// Fielding control modes
typedef enum {
    FIELDING_MODE_ASSISTED = 0,         // Mode A: AI-assisted running + contextual throw
    FIELDING_MODE_MANUAL = 1             // Mode B: Manual thumbstick running + throw buttons
} FieldingControlMode;

// Gameplay Context Phase
typedef enum {
    GAME_PHASE_MENU = 0,
    GAME_PHASE_BATTING = 1,
    GAME_PHASE_BOWLING = 2,
    GAME_PHASE_FIELDING = 3,
    GAME_PHASE_RUNNING = 4
} GamePhase;

// Touch Pointer Ownership Tag
typedef enum {
    OWNER_NONE = 0,
    OWNER_JOYSTICK = 1,
    OWNER_SWIPE_AREA = 2,
    OWNER_BUTTON = 3,
    OWNER_AIM_MARKER = 4
} PointerOwnerTag;

// Virtual Action Button IDs
typedef enum {
    BTN_NONE = 0,
    BTN_LOFT_TOGGLE = 1,
    BTN_ADVANCE = 2,
    BTN_DEFEND = 3,
    BTN_RUN = 4,
    BTN_CANCEL_RUN = 5,
    BTN_BOWL_STANDARD = 6,  // 'S'
    BTN_BOWL_SWING = 7,     // 'A'
    BTN_BOWL_CUT = 8,       // 'D'
    BTN_BOWL_BOUNCER = 9,   // 'W'
    BTN_THROW_KEEPER = 10,
    BTN_THROW_BOWLER = 11,
    BTN_DIVE = 12
} VirtualButtonId;

// Directional Shot Sectors (8 compass sectors)
typedef enum {
    SECTOR_STRAIGHT_DRIVE = 0,  // Up
    SECTOR_COVER_DRIVE = 1,     // Up-Left
    SECTOR_POINT_CUT = 2,       // Left
    SECTOR_THIRD_MAN = 3,       // Down-Left
    SECTOR_FINE_LEG = 4,        // Down-Right
    SECTOR_SQUARE_LEG = 5,      // Right
    SECTOR_MID_WICKET = 6,      // Up-Right (Leg side for RHB)
    SECTOR_MID_ON = 7           // Up-Right
} ShotSector;

// Touch pointer tracking state
typedef struct {
    int id;                     // Android pointerId
    bool active;
    PointerOwnerTag owner;
    float start_x, start_y;
    float current_x, current_y;
    uint64_t start_time_ms;
    uint64_t last_time_ms;
} TouchPointerState;

// Virtual Joystick State
typedef struct {
    float center_x, center_y;
    float radius;
    float deadzone;
    float cur_x, cur_y;
    float norm_x, norm_y;       // -1.0 .. +1.0
    int32_t axis_x, axis_y;     // -32768 .. +32767
    bool is_active;
    int pointer_id;
} VirtualJoystickState;

// Virtual Button State
typedef struct {
    VirtualButtonId id;
    const char *label;
    float x, y, w, h;           // Screen rectangle (pixels)
    bool is_pressed;
    bool is_toggle;
    bool toggle_state;
    uint8_t dik_binding;
    uint32_t pad_binding;
} VirtualButtonState;

// Swipe Stroke Event
typedef struct {
    bool valid;
    ShotSector sector;
    float angle_deg;
    float distance_px;
    float velocity_px_ms;
    uint32_t duration_ms;
    bool is_lofted;
    bool is_defensive;
} SwipeStrokeEvent;

// Mobile Input Configuration
typedef struct {
    BattingControlMode batting_mode;
    BowlingControlMode bowling_mode;
    FieldingControlMode fielding_mode;
    float joystick_deadzone;    // 0.05 .. 0.35 (default 0.15)
    float joystick_sensitivity; // 0.5 .. 2.0 (default 1.0)
    float swipe_sensitivity;    // 0.5 .. 2.0 (default 1.0)
    float button_opacity;       // 0.1 .. 1.0 (default 0.8)
    float button_scale;         // 0.7 .. 1.5 (default 1.0)
    bool left_handed;           // Left-handed UI mirror
    bool haptics_enabled;       // Haptic vibration feedback
    bool auto_run;              // Automatic running assistance
} MobileInputConfig;

// Mobile Input Real-Time Telemetry
typedef struct {
    uint32_t frame_index;
    GamePhase current_phase;
    int active_pointers;
    float joystick_norm_x, joystick_norm_y;
    uint32_t pad_buttons_mask;
    uint8_t active_dik_keys[8];
    int active_dik_count;
    SwipeStrokeEvent last_stroke;
    uint64_t input_latency_us;
} MobileInputTelemetry;

// Core Lifecycle APIs
void mobile_input_init(int screen_w, int screen_h);
void mobile_input_resize(int screen_w, int screen_h);
void mobile_input_set_game_phase(GamePhase phase);
void mobile_input_configure(const MobileInputConfig *config);
void mobile_input_get_config(MobileInputConfig *out_config);

// Android MotionEvent ingress (called from NDK JNI on touch event)
void mobile_input_on_touch(int action, float x, float y, int pointer_id);

// Engine frame tick update (called once per frame before game tick)
void mobile_input_frame_tick(void);

// Telemetry & Feedback
void mobile_input_get_telemetry(MobileInputTelemetry *out_telemetry);
bool mobile_input_poll_haptic_pulse(void);

// Automated Testing Harness APIs
void mobile_input_test_reset(void);
SwipeStrokeEvent mobile_input_test_simulate_swipe(float x0, float y0, float x1, float y1, uint32_t duration_ms);
void mobile_input_test_simulate_joystick(float norm_x, float norm_y);
void mobile_input_test_simulate_button(VirtualButtonId btn, bool pressed);

#ifdef __cplusplus
}
#endif

#endif // CRICKET_MOBILE_INPUT_H
