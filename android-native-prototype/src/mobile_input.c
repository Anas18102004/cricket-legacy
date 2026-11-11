#include "mobile_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <android/log.h>

#define TAG "Cricket-MobileInput"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// External declarations from host/input.c & host/dinput.c
extern void input_key_dik(uint8_t dik, int down);
extern void input_gamepad(int idx, int connected, uint32_t buttons, int lx, int ly, int rx, int ry);
extern void input_tap(int x, int y);
extern void input_tap_move(int dx, int dy);
extern void input_tap_release(void);

// DirectInput scan code constants
#define DIK_ESCAPE          0x01
#define DIK_W               0x11
#define DIK_A               0x1E
#define DIK_S               0x1F
#define DIK_D               0x20
#define DIK_RETURN          0x1C
#define DIK_SPACE           0x39
#define DIK_LCONTROL        0x1D
#define DIK_LSHIFT          0x2A
#define DIK_LMENU           0x38
#define DIK_UP              0xC8
#define DIK_DOWN            0xD0
#define DIK_LEFT            0xCB
#define DIK_RIGHT           0xCD

// Internal State
static pthread_mutex_t s_input_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_screen_w = 2264;
static int s_screen_h = 1017;
// The Android host has no verified Cricket 07 gameplay-state callback yet. Start
// closed so menu touches can never be interpreted as invisible batting controls.
static GamePhase s_current_phase = GAME_PHASE_MENU;
static MobileInputConfig s_config;

static TouchPointerState s_pointers[MOBILE_INPUT_MAX_POINTERS];
static VirtualJoystickState s_joystick;
static VirtualButtonState s_buttons[16];
static int s_button_count = 0;
static const uint8_t s_arrow_dik[4] = {DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT};
static bool s_arrow_down[4];

static SwipeStrokeEvent s_last_stroke;
static bool s_haptic_pulse_pending = false;
static uint32_t s_frame_counter = 0;
static uint64_t s_last_touch_time_us = 0;
static uint64_t s_total_touch_latency_us = 0;
static uint32_t s_touch_latency_samples = 0;

// Stroke pulse timer for engine delivery window
static int s_stroke_key_hold_frames = 0;
static uint8_t s_active_stroke_keys[8];
static int s_active_stroke_key_count = 0;

// High-resolution clock helper (microseconds)
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

static void release_joystick_keys(void) {
    for (int i = 0; i < 4; i++) {
        if (s_arrow_down[i]) input_key_dik(s_arrow_dik[i], 0);
        s_arrow_down[i] = false;
    }
}

// Layout configuration based on screen dimensions and left-handed preference
static void update_hud_layout(void) {
    float sw = (float)s_screen_w;
    float sh = (float)s_screen_h;
    bool left = s_config.left_handed;
    float scale = fminf(sw / 1280.0f, sh / 720.0f);
    if (scale < 0.7f) scale = 0.7f;
    if (scale > 1.4f) scale = 1.4f;
    scale *= s_config.button_scale;

    // 1. Virtual Joystick
    s_joystick.radius = 110.0f * scale;
    s_joystick.deadzone = s_config.joystick_deadzone;
    if (!left) {
        // Right-handed mode: Joystick on bottom-left
        // Match the Android overlay anchor and clear the original delivery meter.
        s_joystick.center_x = 325.0f * scale;
        s_joystick.center_y = sh - 305.0f * scale;
    } else {
        // Left-handed mode: Joystick on bottom-right
        s_joystick.center_x = sw - 180.0f * scale;
        s_joystick.center_y = sh - 305.0f * scale;
    }

    // 2. Virtual Buttons
    s_button_count = 0;
    float btn_sz = 90.0f * scale;
    float gap = 24.0f * scale;

    // Right-side cluster (or left-side if left-handed)
    float cluster_x = !left ? (sw - 240.0f * scale) : (60.0f * scale);
    float cluster_y = sh - 340.0f * scale;

    // Button: Loft Toggle
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_LOFT_TOGGLE,
        .label = "SHIFT",
        .x = cluster_x,
        .y = cluster_y - btn_sz - gap,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .is_toggle = true,
        .toggle_state = false,
        .dik_binding = DIK_LSHIFT,
        .pad_binding = (1u << 4) // LB / L1
    };

    // Button: Advance Down Pitch
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_ADVANCED_SHOT,
        .label = "D",
        .x = cluster_x + btn_sz + gap,
        .y = cluster_y - btn_sz - gap,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .is_toggle = false,
        .dik_binding = DIK_D,
        .pad_binding = (1u << 5) // RB / R1
    };

    // Button: Defend
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_NORMAL_SHOT,
        .label = "S",
        .x = cluster_x - btn_sz - gap,
        .y = cluster_y,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .is_toggle = false,
        .dik_binding = DIK_S,
        .pad_binding = (1u << 3) // Triangle / Y
    };

    // Button: Run
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_RUN,
        .label = "RUN",
        .x = cluster_x,
        .y = cluster_y,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .is_toggle = false,
        .dik_binding = DIK_D,
        .pad_binding = (1u << 0) // Button A
    };

    // Button: Cancel Run
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_CANCEL_RUN,
        .label = "CANCEL",
        .x = cluster_x + btn_sz + gap,
        .y = cluster_y,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .is_toggle = false,
        .dik_binding = DIK_A,
        .pad_binding = (1u << 1) // Button B
    };

    // Bowling Buttons (Cluster in bottom right / center)
    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_BOWL_STANDARD,
        .label = "S",
        .x = cluster_x - btn_sz - gap,
        .y = cluster_y - btn_sz - gap,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .dik_binding = DIK_S,
        .pad_binding = (1u << 0)
    };

    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_BOWL_SWING,
        .label = "A",
        .x = cluster_x,
        .y = cluster_y - btn_sz - gap,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .dik_binding = DIK_A,
        .pad_binding = (1u << 1)
    };

    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_BOWL_CUT,
        .label = "D",
        .x = cluster_x + btn_sz + gap,
        .y = cluster_y - btn_sz - gap,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .dik_binding = DIK_D,
        .pad_binding = (1u << 2)
    };

    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_BOWL_SHIFT,
        .label = "SHIFT",
        .x = cluster_x,
        .y = cluster_y,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .dik_binding = DIK_LSHIFT,
        .pad_binding = (1u << 3)
    };

    s_buttons[s_button_count++] = (VirtualButtonState){
        .id = BTN_BOWL_ALT,
        .label = "ALT",
        .x = cluster_x + btn_sz + gap,
        .y = cluster_y,
        .w = btn_sz,
        .h = btn_sz,
        .is_pressed = false,
        .dik_binding = DIK_LMENU,
        .pad_binding = (1u << 5)
    };

    LOGI("HUD layout updated: screen=%dx%d left_handed=%d button_count=%d",
         s_screen_w, s_screen_h, left, s_button_count);
}

void mobile_input_init(int screen_w, int screen_h) {
    pthread_mutex_lock(&s_input_lock);
    s_screen_w = screen_w > 0 ? screen_w : 2264;
    s_screen_h = screen_h > 0 ? screen_h : 1017;

    // Default configuration
    s_current_phase = GAME_PHASE_MENU;
    s_config.batting_mode = BATTING_MODE_PURE_SWIPE;
    s_config.bowling_mode = BOWLING_MODE_GESTURE_AIM;
    s_config.fielding_mode = FIELDING_MODE_ASSISTED;
    s_config.joystick_deadzone = 0.15f;
    s_config.joystick_sensitivity = 1.0f;
    s_config.swipe_sensitivity = 1.0f;
    s_config.button_opacity = 0.8f;
    s_config.button_scale = 1.0f;
    s_config.left_handed = false;
    s_config.haptics_enabled = true;
    s_config.auto_run = false;

    memset(s_pointers, 0, sizeof(s_pointers));
    memset(&s_joystick, 0, sizeof(s_joystick));
    memset(&s_last_stroke, 0, sizeof(s_last_stroke));

    update_hud_layout();
    pthread_mutex_unlock(&s_input_lock);
    LOGI("Cricket Mobile Input System initialized successfully");
}

void mobile_input_resize(int screen_w, int screen_h) {
    pthread_mutex_lock(&s_input_lock);
    s_screen_w = screen_w;
    s_screen_h = screen_h;
    update_hud_layout();
    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_set_hud_layout(float joystick_x, float joystick_y, float joystick_radius,
                                  const float *button_rects, int button_count) {
    if (!button_rects || button_count <= 0) return;
    pthread_mutex_lock(&s_input_lock);
    release_joystick_keys();
    for (int i = 0; i < s_active_stroke_key_count; i++)
        input_key_dik(s_active_stroke_keys[i], 0);
    s_active_stroke_key_count = 0;
    s_stroke_key_hold_frames = 0;
    for (int i = 0; i < s_button_count; i++) {
        if (s_buttons[i].is_pressed) input_key_dik(s_buttons[i].dik_binding, 0);
        s_buttons[i].is_pressed = false;
    }
    memset(s_pointers, 0, sizeof(s_pointers));
    s_joystick.is_active = false;
    s_joystick.pointer_id = -1;
    s_joystick.norm_x = s_joystick.norm_y = 0.0f;
    s_joystick.axis_x = s_joystick.axis_y = 0;
    input_gamepad(0, 1, 0, 0, 0, 0, 0);
    s_joystick.center_x = joystick_x;
    s_joystick.center_y = joystick_y;
    s_joystick.radius = joystick_radius;
    int count = button_count < s_button_count ? button_count : s_button_count;
    for (int i = 0; i < count; i++) {
        s_buttons[i].x = button_rects[i * 4 + 0];
        s_buttons[i].y = button_rects[i * 4 + 1];
        s_buttons[i].w = button_rects[i * 4 + 2];
        s_buttons[i].h = button_rects[i * 4 + 3];
    }
    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_set_game_phase(GamePhase phase) {
    pthread_mutex_lock(&s_input_lock);
    if (phase < GAME_PHASE_MENU || phase > GAME_PHASE_REPLAY) {
        phase = GAME_PHASE_MENU;
    }
    s_current_phase = phase;
    // Explicit context changes release all transient input and pointer owners.
    // This is user-driven state, not an inferred Cricket 07 phase.
    for (int i = 0; i < s_active_stroke_key_count; i++) {
        input_key_dik(s_active_stroke_keys[i], 0);
    }
    s_active_stroke_key_count = 0;
    s_stroke_key_hold_frames = 0;
    release_joystick_keys();
    for (int b = 0; b < s_button_count; b++) {
        VirtualButtonState *btn = &s_buttons[b];
        if (btn->is_pressed || btn->toggle_state) input_key_dik(btn->dik_binding, 0);
        btn->is_pressed = false;
        btn->toggle_state = false;
    }
    memset(s_pointers, 0, sizeof(s_pointers));
    s_joystick.is_active = false;
    s_joystick.norm_x = s_joystick.norm_y = 0.0f;
    s_joystick.axis_x = s_joystick.axis_y = 0;
    s_joystick.pointer_id = -1;
    input_gamepad(0, 1, 0, 0, 0, 0, 0);
    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_configure(const MobileInputConfig *config) {
    if (!config) return;
    pthread_mutex_lock(&s_input_lock);
    s_config = *config;
    s_config.batting_mode = config->batting_mode == BATTING_MODE_CLASSIC_JOYSTICK
        ? BATTING_MODE_CLASSIC_JOYSTICK : BATTING_MODE_PURE_SWIPE;
    update_hud_layout();
    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_get_config(MobileInputConfig *out_config) {
    if (!out_config) return;
    pthread_mutex_lock(&s_input_lock);
    *out_config = s_config;
    pthread_mutex_unlock(&s_input_lock);
}

// 8-Sector Direction Classifier from 2D angle
static ShotSector classify_shot_sector(float dx, float dy) {
    // dy is down-positive on Android screen, invert for math angle
    float angle_rad = atan2f(-dy, dx);
    float angle_deg = angle_rad * (180.0f / (float)M_PI);
    if (angle_deg < 0.0f) angle_deg += 360.0f;

    // 8 sectors centered at 0, 45, 90, 135, 180, 225, 270, 315 degrees
    if (angle_deg >= 67.5f && angle_deg < 112.5f) {
        return SECTOR_STRAIGHT_DRIVE;   // ~90 deg (Up)
    } else if (angle_deg >= 112.5f && angle_deg < 157.5f) {
        return SECTOR_COVER_DRIVE;      // ~135 deg (Up-Left)
    } else if (angle_deg >= 157.5f && angle_deg < 202.5f) {
        return SECTOR_POINT_CUT;        // ~180 deg (Left)
    } else if (angle_deg >= 202.5f && angle_deg < 247.5f) {
        return SECTOR_THIRD_MAN;        // ~225 deg (Down-Left)
    } else if (angle_deg >= 247.5f && angle_deg < 292.5f) {
        return SECTOR_FINE_LEG;         // ~270 deg (Down)
    } else if (angle_deg >= 292.5f && angle_deg < 337.5f) {
        return SECTOR_SQUARE_LEG;       // ~315 deg (Down-Right)
    } else if (angle_deg >= 337.5f || angle_deg < 22.5f) {
        return SECTOR_MID_WICKET;       // ~0 deg (Right)
    } else {
        return SECTOR_MID_ON;           // ~45 deg (Up-Right)
    }
}

// Updates virtual joystick axis state and dispatches gamepad input
static void update_joystick_axis(float x, float y) {
    s_joystick.cur_x = x;
    s_joystick.cur_y = y;
    float jdx = x - s_joystick.center_x;
    float jdy = y - s_joystick.center_y;
    float dist = sqrtf(jdx * jdx + jdy * jdy);
    float max_r = s_joystick.radius;

    if (dist > max_r) {
        jdx = (jdx / dist) * max_r;
        jdy = (jdy / dist) * max_r;
        dist = max_r;
    }

    float dead_px = max_r * s_joystick.deadzone;
    if (dist < dead_px) {
        s_joystick.norm_x = 0.0f;
        s_joystick.norm_y = 0.0f;
        s_joystick.axis_x = 0;
        s_joystick.axis_y = 0;
    } else {
        float active_range = max_r - dead_px;
        float factor = ((dist - dead_px) / active_range) * s_config.joystick_sensitivity;
        if (factor > 1.0f) factor = 1.0f;
        s_joystick.norm_x = (jdx / dist) * factor;
        s_joystick.norm_y = (jdy / dist) * factor;
        s_joystick.axis_x = (int32_t)(s_joystick.norm_x * 32767.0f);
        s_joystick.axis_y = (int32_t)(s_joystick.norm_y * 32767.0f);
    }
    // The reference touch control emits keyboard arrows, including diagonals.
    bool want[4] = {false, false, false, false};
    if (dist >= dead_px && dist > 0.0f) {
        int sector = (int)lroundf(atan2f(jdy, jdx) / ((float)M_PI / 4.0f));
        sector = (sector % 8 + 8) % 8; /* right, down-right, down, ... */
        switch (sector) {
            case 0: want[3] = true; break;
            case 1: want[1] = want[3] = true; break;
            case 2: want[1] = true; break;
            case 3: want[1] = want[2] = true; break;
            case 4: want[2] = true; break;
            case 5: want[0] = want[2] = true; break;
            case 6: want[0] = true; break;
            case 7: want[0] = want[3] = true; break;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (want[i] != s_arrow_down[i]) {
            input_key_dik(s_arrow_dik[i], want[i] ? 1 : 0);
            s_arrow_down[i] = want[i];
        }
    }
}

static bool button_allowed_in_context(GamePhase phase, VirtualButtonId id) {
    if (phase == GAME_PHASE_BATTING || phase == GAME_PHASE_BALL_DEAD)
        return id == BTN_LOFT_TOGGLE || id == BTN_ADVANCED_SHOT || id == BTN_NORMAL_SHOT;
    if (phase == GAME_PHASE_BOWLING)
        return id >= BTN_BOWL_STANDARD && id <= BTN_BOWL_ALT;
    if (phase == GAME_PHASE_RUNNING)
        return id == BTN_RUN || id == BTN_CANCEL_RUN;
    return false;
}

// Maps ShotSector to original DirectInput scan codes
static void map_sector_to_dik(ShotSector sector, uint8_t *out_keys, int *out_count) {
    *out_count = 0;
    switch (sector) {
    case SECTOR_STRAIGHT_DRIVE:
        out_keys[(*out_count)++] = DIK_UP;
        break;
    case SECTOR_COVER_DRIVE:
        out_keys[(*out_count)++] = DIK_UP;
        out_keys[(*out_count)++] = DIK_LEFT;
        break;
    case SECTOR_POINT_CUT:
        out_keys[(*out_count)++] = DIK_LEFT;
        break;
    case SECTOR_THIRD_MAN:
        out_keys[(*out_count)++] = DIK_DOWN;
        out_keys[(*out_count)++] = DIK_LEFT;
        break;
    case SECTOR_FINE_LEG:
        out_keys[(*out_count)++] = DIK_DOWN;
        break;
    case SECTOR_SQUARE_LEG:
        out_keys[(*out_count)++] = DIK_DOWN;
        out_keys[(*out_count)++] = DIK_RIGHT;
        break;
    case SECTOR_MID_WICKET:
        out_keys[(*out_count)++] = DIK_RIGHT;
        break;
    case SECTOR_MID_ON:
        out_keys[(*out_count)++] = DIK_UP;
        out_keys[(*out_count)++] = DIK_RIGHT;
        break;
    }
}

// Executes a recognized stroke through DirectInput injection
static void dispatch_batting_stroke(SwipeStrokeEvent stroke) {
    s_last_stroke = stroke;
    uint8_t dir_keys[4];
    int dir_count = 0;
    map_sector_to_dik(stroke.sector, dir_keys, &dir_count);

    // Apply direction keys
    for (int i = 0; i < dir_count; i++) {
        input_key_dik(dir_keys[i], 1);
        s_active_stroke_keys[i] = dir_keys[i];
    }
    s_active_stroke_key_count = dir_count;

    // Apply Loft modifier if requested
    if (stroke.is_lofted) {
        input_key_dik(DIK_LSHIFT, 1);
        s_active_stroke_keys[s_active_stroke_key_count++] = DIK_LSHIFT;
    }

    // The browser control path uses S as the normal shot key. Direction and
    // optional loft are held alongside it; host assertion is not action proof.
    input_key_dik(DIK_S, 1);
    s_active_stroke_keys[s_active_stroke_key_count++] = DIK_S;

    // Six host adapter callbacks keep this synthetic swipe pulse bounded. This
    // duration does not guarantee a sample by the original game thread.
    s_stroke_key_hold_frames = 6;
    s_haptic_pulse_pending = true;

    LOGI("[Stroke Dispatched] Sector=%d Angle=%.1f deg Dist=%.1f px Vel=%.2f Loft=%d",
         stroke.sector, stroke.angle_deg, stroke.distance_px, stroke.velocity_px_ms, stroke.is_lofted);
}

// Ingress handler for Android MotionEvents
void mobile_input_on_touch(int action, float x, float y, int pointer_id) {
    uint64_t t0 = get_time_us();
    pthread_mutex_lock(&s_input_lock);

    // The Android caller routes only visible HUD touches here. Context is an
    // explicit user choice; it does not claim to reflect engine state.
    if (s_current_phase == GAME_PHASE_MENU || s_current_phase == GAME_PHASE_FIELDING ||
        s_current_phase == GAME_PHASE_REPLAY) {
        pthread_mutex_unlock(&s_input_lock);
        return;
    }

    int masked_action = action & 0xFF; // MotionEvent.ACTION_MASK
    if (masked_action == 3 /* ACTION_CANCEL */) {
        release_joystick_keys();
        for (int b = 0; b < s_button_count; b++) {
            VirtualButtonState *btn = &s_buttons[b];
            if (btn->is_pressed || btn->toggle_state) input_key_dik(btn->dik_binding, 0);
            btn->is_pressed = false;
            btn->toggle_state = false;
        }
        for (int i = 0; i < s_active_stroke_key_count; i++)
            input_key_dik(s_active_stroke_keys[i], 0);
        s_active_stroke_key_count = 0;
        s_stroke_key_hold_frames = 0;
        memset(s_pointers, 0, sizeof(s_pointers));
        s_joystick.is_active = false;
        s_joystick.norm_x = s_joystick.norm_y = 0.0f;
        s_joystick.axis_x = s_joystick.axis_y = 0;
        s_joystick.pointer_id = -1;
        input_gamepad(0, 1, 0, 0, 0, 0, 0);
        pthread_mutex_unlock(&s_input_lock);
        return;
    }

    // Map pointer slot
    TouchPointerState *ptr = NULL;
    for (int i = 0; i < MOBILE_INPUT_MAX_POINTERS; i++) {
        if (s_pointers[i].active && s_pointers[i].id == pointer_id) {
            ptr = &s_pointers[i];
            break;
        }
    }

    if (masked_action == 0 /* ACTION_DOWN */ || masked_action == 5 /* ACTION_POINTER_DOWN */) {
        // Allocate slot for new pointer
        if (!ptr) {
            for (int i = 0; i < MOBILE_INPUT_MAX_POINTERS; i++) {
                if (!s_pointers[i].active) {
                    ptr = &s_pointers[i];
                    break;
                }
            }
        }
        if (ptr) {
            ptr->id = pointer_id;
            ptr->active = true;
            ptr->start_x = ptr->current_x = x;
            ptr->start_y = ptr->current_y = y;
            ptr->start_time_ms = ptr->last_time_ms = get_time_ms();
            ptr->owner = OWNER_NONE;

            // 1. Check Button Bounds
            for (int b = 0; b < s_button_count; b++) {
                VirtualButtonState *btn = &s_buttons[b];
                if (!button_allowed_in_context(s_current_phase, btn->id)) continue;
                float slop = 12.0f; // touch slop margin
                if (x >= btn->x - slop && x <= btn->x + btn->w + slop &&
                    y >= btn->y - slop && y <= btn->y + btn->h + slop) {
                    ptr->owner = OWNER_BUTTON;
                    ptr->button_id = (int)btn->id;
                    btn->is_pressed = true;
                    if (btn->is_toggle) {
                        btn->toggle_state = !btn->toggle_state;
                        input_key_dik(btn->dik_binding, btn->toggle_state ? 1 : 0);
                    } else {
                        input_key_dik(btn->dik_binding, 1);
                        input_gamepad(0, 1, btn->pad_binding, s_joystick.axis_x, s_joystick.axis_y, 0, 0);
                    }
                    s_haptic_pulse_pending = true;
                    break;
                }
            }

            // 2. Check Virtual Joystick Zone
            if (ptr->owner == OWNER_NONE) {
                float jdx = x - s_joystick.center_x;
                float jdy = y - s_joystick.center_y;
                float jdist = sqrtf(jdx * jdx + jdy * jdy);
    bool joystick_context = (s_current_phase == GAME_PHASE_BOWLING) ||
        ((s_current_phase == GAME_PHASE_BATTING || s_current_phase == GAME_PHASE_BALL_DEAD) &&
         s_config.batting_mode == BATTING_MODE_CLASSIC_JOYSTICK);
                if (joystick_context && jdist <= s_joystick.radius * 1.5f) {
                    ptr->owner = OWNER_JOYSTICK;
                    s_joystick.is_active = true;
                    s_joystick.pointer_id = pointer_id;
                    update_joystick_axis(x, y);
                } else {
                    // 3. Otherwise, assign to Swipe Area
                    ptr->owner = OWNER_SWIPE_AREA;
                }
            }
            LOGI("[HUD owner] DOWN pointer=%d owner=%d phase=%d button=%d",
                 pointer_id, ptr->owner, s_current_phase, ptr->button_id);
        }
    } else if (masked_action == 2 /* ACTION_MOVE */) {
        if (ptr && ptr->active) {
            ptr->current_x = x;
            ptr->current_y = y;
            ptr->last_time_ms = get_time_ms();

            if (ptr->owner == OWNER_JOYSTICK) {
                update_joystick_axis(x, y);
            }
        }
    } else if (masked_action == 1 /* ACTION_UP */ || masked_action == 6 /* ACTION_POINTER_UP */ || masked_action == 3 /* ACTION_CANCEL */) {
        if (ptr && ptr->active) {
            LOGI("[HUD owner] UP pointer=%d owner=%d button=%d",
                 pointer_id, ptr->owner, ptr->button_id);
            uint64_t now_ms = get_time_ms();
            uint32_t duration_ms = (uint32_t)(now_ms - ptr->start_time_ms);
            float dx = x - ptr->start_x;
            float dy = y - ptr->start_y;
            float dist = sqrtf(dx * dx + dy * dy);
            float vel = (duration_ms > 0) ? (dist / (float)duration_ms) : 0.0f;

            if (ptr->owner == OWNER_SWIPE_AREA &&
                (s_current_phase == GAME_PHASE_BATTING || s_current_phase == GAME_PHASE_BALL_DEAD) &&
                s_config.batting_mode == BATTING_MODE_PURE_SWIPE) {
                // Evaluate Swipe Stroke
                if (dist >= 25.0f * s_config.swipe_sensitivity && duration_ms < 600) {
                    SwipeStrokeEvent stroke;
                    stroke.valid = true;
                    stroke.distance_px = dist;
                    stroke.velocity_px_ms = vel;
                    stroke.duration_ms = duration_ms;
                    stroke.sector = classify_shot_sector(dx, dy);

                    float angle_rad = atan2f(-dy, dx);
                    stroke.angle_deg = angle_rad * (180.0f / (float)M_PI);
                    if (stroke.angle_deg < 0.0f) stroke.angle_deg += 360.0f;

                    // Determine Loft state from toggle or high velocity
                    stroke.is_lofted = s_buttons[0].toggle_state || (vel > 2.2f);
                    stroke.is_defensive = false;

                    dispatch_batting_stroke(stroke);
                } else if (dist < 15.0f && duration_ms < 150) {
                    // Short tap = Defensive block
                    SwipeStrokeEvent defense = {
                        .valid = true,
                        .sector = SECTOR_STRAIGHT_DRIVE,
                        .angle_deg = 90.0f,
                        .distance_px = dist,
                        .velocity_px_ms = vel,
                        .duration_ms = duration_ms,
                        .is_lofted = false,
                        .is_defensive = true
                    };
                    input_key_dik(DIK_S, 1);
                    s_active_stroke_keys[0] = DIK_S;
                    s_active_stroke_key_count = 1;
                    s_stroke_key_hold_frames = 6;
                    s_haptic_pulse_pending = true;
                    s_last_stroke = defense;
                    LOGI("[Defense Block Dispatched]");
                }
            } else if (ptr->owner == OWNER_JOYSTICK) {
                release_joystick_keys();
                s_joystick.is_active = false;
                s_joystick.norm_x = s_joystick.norm_y = 0.0f;
                s_joystick.axis_x = s_joystick.axis_y = 0;
                input_gamepad(0, 1, 0, 0, 0, 0, 0);
            } else if (ptr->owner == OWNER_BUTTON) {
                for (int b = 0; b < s_button_count; b++) {
                    VirtualButtonState *btn = &s_buttons[b];
                    if ((int)btn->id == ptr->button_id && !btn->is_toggle) {
                        btn->is_pressed = false;
                        input_key_dik(btn->dik_binding, 0);
                        input_gamepad(0, 1, 0, s_joystick.axis_x, s_joystick.axis_y, 0, 0);
                        break;
                    }
                }
            }

            ptr->active = false;
            ptr->owner = OWNER_NONE;
        }
    }

    uint64_t t1 = get_time_us();
    s_last_touch_time_us = t1 - t0;
    s_total_touch_latency_us += s_last_touch_time_us;
    s_touch_latency_samples++;

    pthread_mutex_unlock(&s_input_lock);
}

// Host adapter tick (called from the Android render callback, not an engine poll).
void mobile_input_frame_tick(void) {
    pthread_mutex_lock(&s_input_lock);
    s_frame_counter++;

    // Decrement hold timers for stroke input keys
    if (s_stroke_key_hold_frames > 0) {
        s_stroke_key_hold_frames--;
        if (s_stroke_key_hold_frames == 0) {
            for (int i = 0; i < s_active_stroke_key_count; i++) {
                input_key_dik(s_active_stroke_keys[i], 0);
            }
            s_active_stroke_key_count = 0;
            // Also release gamepad button A
            input_gamepad(0, 1, 0, s_joystick.axis_x, s_joystick.axis_y, 0, 0);
        }
    }

    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_get_telemetry(MobileInputTelemetry *out_telemetry) {
    if (!out_telemetry) return;
    pthread_mutex_lock(&s_input_lock);

    out_telemetry->frame_index = s_frame_counter;
    out_telemetry->current_phase = s_current_phase;
    out_telemetry->active_pointers = 0;
    for (int i = 0; i < MOBILE_INPUT_MAX_POINTERS; i++) {
        if (s_pointers[i].active) out_telemetry->active_pointers++;
    }
    out_telemetry->joystick_norm_x = s_joystick.norm_x;
    out_telemetry->joystick_norm_y = s_joystick.norm_y;
    out_telemetry->last_stroke = s_last_stroke;
    out_telemetry->input_latency_us = (s_touch_latency_samples > 0) ?
        (s_total_touch_latency_us / s_touch_latency_samples) : 0;

    pthread_mutex_unlock(&s_input_lock);
}

bool mobile_input_poll_haptic_pulse(void) {
    pthread_mutex_lock(&s_input_lock);
    bool pulse = s_haptic_pulse_pending && s_config.haptics_enabled;
    s_haptic_pulse_pending = false;
    pthread_mutex_unlock(&s_input_lock);
    return pulse;
}

// ============================================================================
// Automated Testing APIs
// ============================================================================

void mobile_input_test_reset(void) {
    pthread_mutex_lock(&s_input_lock);
    release_joystick_keys();
    memset(s_pointers, 0, sizeof(s_pointers));
    s_joystick.is_active = false;
    s_joystick.norm_x = s_joystick.norm_y = 0.0f;
    s_joystick.axis_x = s_joystick.axis_y = 0;
    s_joystick.cur_x = s_joystick.center_x;
    s_joystick.cur_y = s_joystick.center_y;
    s_joystick.pointer_id = -1;
    memset(&s_last_stroke, 0, sizeof(s_last_stroke));
    s_stroke_key_hold_frames = 0;
    s_active_stroke_key_count = 0;
    update_hud_layout();
    pthread_mutex_unlock(&s_input_lock);
}

SwipeStrokeEvent mobile_input_test_simulate_swipe(float x0, float y0, float x1, float y1, uint32_t duration_ms) {
    mobile_input_on_touch(0 /* ACTION_DOWN */, x0, y0, 0);
    // Move mid point
    mobile_input_on_touch(2 /* ACTION_MOVE */, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, 0);
    // Release
    mobile_input_on_touch(1 /* ACTION_UP */, x1, y1, 0);

    MobileInputTelemetry telem;
    mobile_input_get_telemetry(&telem);
    return telem.last_stroke;
}

void mobile_input_test_simulate_joystick(float norm_x, float norm_y) {
    pthread_mutex_lock(&s_input_lock);
    s_joystick.norm_x = norm_x;
    s_joystick.norm_y = norm_y;
    s_joystick.axis_x = (int32_t)(norm_x * 32767.0f);
    s_joystick.axis_y = (int32_t)(norm_y * 32767.0f);
    input_gamepad(0, 1, 0, s_joystick.axis_x, s_joystick.axis_y, 0, 0);
    pthread_mutex_unlock(&s_input_lock);
}

void mobile_input_test_simulate_button(VirtualButtonId btn_id, bool pressed) {
    pthread_mutex_lock(&s_input_lock);
    for (int i = 0; i < s_button_count; i++) {
        if (s_buttons[i].id == btn_id) {
            s_buttons[i].is_pressed = pressed;
            input_key_dik(s_buttons[i].dik_binding, pressed ? 1 : 0);
            break;
        }
    }
    pthread_mutex_unlock(&s_input_lock);
}
