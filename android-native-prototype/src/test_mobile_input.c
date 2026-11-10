#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "mobile_input.h"

// Mock host input functions
static uint8_t s_mock_dik_state[256];
static int32_t s_mock_pad_lx = 0, s_mock_pad_ly = 0;
static uint32_t s_mock_pad_btn = 0;
static int s_mock_pad_updates = 0;

void input_key_dik(uint8_t dik, int down) {
    s_mock_dik_state[dik] = down ? 1 : 0;
    // printf("[MOCK DIK] key=0x%02X down=%d\n", dik, down);
}

void input_gamepad(int idx, int connected, uint32_t buttons, int lx, int ly, int rx, int ry) {
    (void)idx;
    (void)connected;
    (void)rx;
    (void)ry;
    s_mock_pad_btn = buttons;
    s_mock_pad_lx = lx;
    s_mock_pad_ly = ly;
    s_mock_pad_updates++;
}

void input_tap(int x, int y) { (void)x; (void)y; }
void input_tap_move(int dx, int dy) { (void)dx; (void)dy; }
void input_tap_release(void) {}

// Gameplay input is deliberately disabled until the host reports an explicit
// batting context. Test batting mechanics only after opting into that context.
static void prepare_batting_input(BattingControlMode mode) {
    mobile_input_init(1920, 1080);
    MobileInputConfig config;
    mobile_input_get_config(&config);
    config.batting_mode = mode;
    mobile_input_configure(&config);
    mobile_input_set_game_phase(GAME_PHASE_BATTING);
    mobile_input_test_reset();
    memset(s_mock_dik_state, 0, sizeof(s_mock_dik_state));
    s_mock_pad_lx = s_mock_pad_ly = 0;
    s_mock_pad_btn = 0;
    s_mock_pad_updates = 0;
}

static void test_menu_context_is_fail_closed(void) {
    printf("--- [TEST 0] Menu Context Fails Closed ---\n");
    mobile_input_init(1920, 1080);
    mobile_input_test_reset();

    mobile_input_on_touch(0 /* DOWN */, 960.0f, 540.0f, 0);
    mobile_input_on_touch(2 /* MOVE */, 960.0f, 440.0f, 0);
    mobile_input_on_touch(1 /* UP */, 960.0f, 440.0f, 0);

    MobileInputTelemetry telem;
    mobile_input_get_telemetry(&telem);
    assert(!telem.last_stroke.valid);
    assert(s_mock_dik_state[0x1C] == 0);
    assert(s_mock_pad_btn == 0);
    printf("  Menu swipe produced no gameplay action -> PASS\n\n");
}

// Test 1: 8-Sector Direction Classifier & DirectInput mapping
static void test_sector_classification(void) {
    printf("--- [TEST 1] 8-Sector Directional Classification & Injection ---\n");
    prepare_batting_input(BATTING_MODE_PURE_SWIPE);

    struct SectorTestCase {
        float dx, dy;
        ShotSector expected_sector;
        const char *name;
        uint8_t primary_key;
    } cases[] = {
        { 0.0f, -100.0f, SECTOR_STRAIGHT_DRIVE, "Straight Drive", 0xC8 /* DIK_UP */ },
        { -70.0f, -70.0f, SECTOR_COVER_DRIVE, "Cover Drive", 0xC8 /* DIK_UP */ },
        { -100.0f, 0.0f, SECTOR_POINT_CUT, "Square Cut", 0xCB /* DIK_LEFT */ },
        { -70.0f, 70.0f, SECTOR_THIRD_MAN, "Third Man / Late Cut", 0xD0 /* DIK_DOWN */ },
        { 0.0f, 100.0f, SECTOR_FINE_LEG, "Fine Leg Glance", 0xD0 /* DIK_DOWN */ },
        { 70.0f, 70.0f, SECTOR_SQUARE_LEG, "Square Leg Flick", 0xCD /* DIK_RIGHT */ },
        { 100.0f, 0.0f, SECTOR_MID_WICKET, "Mid-Wicket Pull", 0xC8 /* DIK_UP */ },
        { 70.0f, -70.0f, SECTOR_MID_ON, "Mid-On Drive", 0xC8 /* DIK_UP */ },
    };

    int num_cases = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < num_cases; i++) {
        memset(s_mock_dik_state, 0, sizeof(s_mock_dik_state));
        float start_x = 960.0f;
        float start_y = 540.0f;
        float end_x = start_x + cases[i].dx;
        float end_y = start_y + cases[i].dy;

        // Perform gesture
        mobile_input_on_touch(0 /* DOWN */, start_x, start_y, 0);
        mobile_input_on_touch(2 /* MOVE */, (start_x + end_x) * 0.5f, (start_y + end_y) * 0.5f, 0);
        mobile_input_on_touch(1 /* UP */, end_x, end_y, 0);

        MobileInputTelemetry telem;
        mobile_input_get_telemetry(&telem);

        printf("  Sector Test [%d]: '%s' -> Detected Sector=%d (Expected=%d) Angle=%.1f deg\n",
               i, cases[i].name, telem.last_stroke.sector, cases[i].expected_sector, telem.last_stroke.angle_deg);
        assert(telem.last_stroke.sector == cases[i].expected_sector);
        assert(s_mock_dik_state[cases[i].primary_key] == 1); // Primary direction key must be down
        assert(s_mock_dik_state[0x1C] == 1); // Enter shot trigger must be down

        // Simulate frame ticks to verify clean key release after 6 frames
        for (int f = 0; f < 6; f++) {
            mobile_input_frame_tick();
        }
        assert(s_mock_dik_state[0x1C] == 0); // Must be released
        assert(s_mock_dik_state[cases[i].primary_key] == 0); // Must be released
    }
    printf("  --> PASS: All 8 directional shot sectors verified!\n\n");
}

// Test 2: Deadzone & Sensitivity processing for Virtual Joystick
static void test_joystick_deadzone(void) {
    printf("--- [TEST 2] Joystick Deadzone & Linear Scaling ---\n");
    prepare_batting_input(BATTING_MODE_CLASSIC_JOYSTICK);

    // 1920x1080 layout scale clamps to 1.4; joystick anchor is 325px * scale.
    float joy_cx = 325.0f * 1.4f;
    float joy_cy = 1080.0f - 305.0f * 1.4f;

    mobile_input_on_touch(0 /* DOWN */, joy_cx + 5.0f, joy_cy + 5.0f, 0);
    mobile_input_on_touch(2 /* MOVE */, joy_cx + 5.0f, joy_cy + 5.0f, 0);

    assert(s_mock_pad_lx == 0);
    assert(s_mock_pad_ly == 0);
    printf("  Sub-deadzone touch (5px): Axis=(%d, %d) -> PASS (Zero output)\n", s_mock_pad_lx, s_mock_pad_ly);

    // Touch outside deadzone: 60px right
    mobile_input_on_touch(2 /* MOVE */, joy_cx + 60.0f, joy_cy, 0);
    assert(s_mock_pad_lx > 5000);
    printf("  Active range touch (60px): Axis=(%d, %d) -> PASS (Smooth scaled output)\n", s_mock_pad_lx, s_mock_pad_ly);

    // Release joystick
    mobile_input_on_touch(1 /* UP */, joy_cx + 60.0f, joy_cy, 0);
    assert(s_mock_pad_lx == 0);
    assert(s_mock_pad_ly == 0);
    printf("  Joystick release: Axis=(%d, %d) -> PASS (Centered)\n", s_mock_pad_lx, s_mock_pad_ly);
    printf("  --> PASS: Joystick deadzone and scaling verified!\n\n");
}

// Test 3: Multi-Touch Pointer Independence (Simultaneous Joystick + Button + Swipe)
static void test_multitouch_concurrency(void) {
    printf("--- [TEST 3] Multi-Touch Pointer Tagging & Concurrency ---\n");
    prepare_batting_input(BATTING_MODE_CLASSIC_JOYSTICK);

    float scale = 1.4f;
    float joy_cx = 325.0f * scale;
    float joy_cy = 1080.0f - 305.0f * scale;

    // Pointer 0: Joystick touch (held down)
    mobile_input_on_touch(0 /* DOWN */, joy_cx + 70.0f, joy_cy, 0);
    assert(s_mock_pad_lx > 0);

    // Pointer 1: Tap LOFT button at the responsive right-side layout location.
    float cluster_x = 1920.0f - 240.0f * scale;
    float cluster_y = 1080.0f - 340.0f * scale;
    float loft_x = cluster_x + 20.0f;
    float loft_y = cluster_y - 90.0f * scale - 24.0f * scale + 20.0f;
    mobile_input_on_touch(5 /* POINTER_DOWN */, loft_x, loft_y, 1);
    assert(s_mock_dik_state[0x2A] == 1); // DIK_LSHIFT active (Loft enabled)
    printf("  Pointer 0 (Joystick) + Pointer 1 (LOFT Button) active concurrently: Stick LX=%d, DIK_LSHIFT=%d -> PASS\n",
           s_mock_pad_lx, s_mock_dik_state[0x2A]);

    // Classic mode must not turn an unrelated screen swipe into a shot.
    mobile_input_on_touch(5 /* POINTER_DOWN */, 960.0f, 540.0f, 2);
    mobile_input_on_touch(2 /* MOVE */, 960.0f, 440.0f, 2);
    mobile_input_on_touch(6 /* POINTER_UP */, 960.0f, 440.0f, 2);
    MobileInputTelemetry telem;
    mobile_input_get_telemetry(&telem);
    assert(!telem.last_stroke.valid);
    assert(s_mock_dik_state[0x1C] == 0);
    printf("  Classic joystick mode ignores unrelated swipe -> PASS\n");

    // Release Pointer 0
    mobile_input_on_touch(1 /* UP */, joy_cx + 70.0f, joy_cy, 0);
    assert(s_mock_pad_lx == 0);
    printf("  --> PASS: Multi-touch isolation and concurrent processing verified!\n\n");
}

static void test_user_context_button_routing(void) {
    printf("--- [TEST 4] User Context Routes Only Its Controls ---\n");
    mobile_input_init(1920, 1080);
    MobileInputConfig config;
    mobile_input_get_config(&config);
    mobile_input_configure(&config);
    memset(s_mock_dik_state, 0, sizeof(s_mock_dik_state));

    float scale = 1.4f;
    float cluster_x = 1920.0f - 240.0f * scale;
    float cluster_y = 1080.0f - 340.0f * scale;
    float btn = 90.0f * scale;
    float gap = 24.0f * scale;

    mobile_input_set_game_phase(GAME_PHASE_RUNNING);
    mobile_input_on_touch(0, cluster_x + btn * 0.5f, cluster_y + btn * 0.5f, 0);
    assert(s_mock_dik_state[0x20] == 1); // Candidate RUN binding reaches host DIK injection.
    mobile_input_on_touch(1, cluster_x + btn * 0.5f, cluster_y + btn * 0.5f, 0);
    assert(s_mock_dik_state[0x20] == 0);
    assert(s_mock_dik_state[0x1F] == 0);

    mobile_input_set_game_phase(GAME_PHASE_BOWLING);
    float standard_x = cluster_x - btn - gap + btn * 0.5f;
    float standard_y = cluster_y - btn - gap + btn * 0.5f;
    mobile_input_on_touch(0, standard_x, standard_y, 0);
    assert(s_mock_dik_state[0x1F] == 1); // Candidate standard-bowl binding.
    mobile_input_on_touch(1, standard_x, standard_y, 0);
    assert(s_mock_dik_state[0x1F] == 0);

    mobile_input_set_game_phase(GAME_PHASE_MENU);
    mobile_input_on_touch(0, standard_x, standard_y, 0);
    assert(s_mock_dik_state[0x1F] == 0);
    printf("  RUN/bowling controls route to host input; menu context stays closed -> PASS\n\n");
}

static void test_cancel_and_context_change_release_inputs(void) {
    printf("--- [TEST 5] Cancel and Context Change Release Inputs ---\n");
    prepare_batting_input(BATTING_MODE_CLASSIC_JOYSTICK);
    float scale = 1.4f;
    float joy_x = 325.0f * scale;
    float joy_y = 1080.0f - 305.0f * scale;
    mobile_input_on_touch(0, joy_x + 70.0f, joy_y, 0);
    assert(s_mock_pad_lx > 0);
    mobile_input_on_touch(3 /* ACTION_CANCEL */, joy_x + 70.0f, joy_y, 0);
    assert(s_mock_pad_lx == 0 && s_mock_pad_ly == 0);

    mobile_input_set_game_phase(GAME_PHASE_RUNNING);
    float cluster_x = 1920.0f - 240.0f * scale;
    float cluster_y = 1080.0f - 340.0f * scale;
    float btn = 90.0f * scale;
    mobile_input_on_touch(0, cluster_x + btn * 0.5f, cluster_y + btn * 0.5f, 1);
    assert(s_mock_dik_state[0x20] == 1);
    mobile_input_set_game_phase(GAME_PHASE_MENU);
    assert(s_mock_dik_state[0x20] == 0);
    assert(s_mock_pad_lx == 0 && s_mock_pad_ly == 0);
    printf("  ACTION_CANCEL and context change release joystick/key states -> PASS\n\n");
}

static void test_android_viewport_layout_is_used_for_hit_testing(void) {
    printf("--- [TEST 6] Android Viewport Layout Drives Native Hit Tests ---\n");
    prepare_batting_input(BATTING_MODE_CLASSIC_JOYSTICK);

    float rects[9 * 4];
    for (int i = 0; i < 9; i++) {
        rects[i * 4 + 0] = 700.0f + (float)(i % 3) * 100.0f;
        rects[i * 4 + 1] = 600.0f + (float)(i / 3) * 100.0f;
        rects[i * 4 + 2] = 80.0f;
        rects[i * 4 + 3] = 80.0f;
    }
    mobile_input_set_hud_layout(350.0f, 780.0f, 60.0f, rects, 9);

    mobile_input_on_touch(0, 400.0f, 780.0f, 0);
    assert(s_mock_pad_lx > 10000);
    mobile_input_on_touch(1, 400.0f, 780.0f, 0);
    assert(s_mock_pad_lx == 0 && s_mock_pad_ly == 0);

    // The injected RUN rectangle is at x=700,y=700, independent of defaults.
    mobile_input_set_game_phase(GAME_PHASE_RUNNING);
    mobile_input_on_touch(0, 740.0f, 740.0f, 1);
    assert(s_mock_dik_state[0x20] == 1);
    mobile_input_on_touch(1, 740.0f, 740.0f, 1);
    assert(s_mock_dik_state[0x20] == 0);
    printf("  Injected viewport joystick and RUN bounds drive native input -> PASS\n\n");
}

int main(void) {
    printf("==================================================================\n");
    printf("CRICKET LEGACY: Native Mobile Input System Unit & Integration Test\n");
    printf("==================================================================\n\n");

    test_menu_context_is_fail_closed();
    test_sector_classification();
    test_joystick_deadzone();
    test_multitouch_concurrency();
    test_user_context_button_routing();
    test_cancel_and_context_change_release_inputs();
    test_android_viewport_layout_is_used_for_hit_testing();

    printf("==================================================================\n");
    printf("ALL MOBILE INPUT UNIT TESTS COMPLETED SUCCESSFULLY!\n");
    printf("==================================================================\n");
    return 0;
}
