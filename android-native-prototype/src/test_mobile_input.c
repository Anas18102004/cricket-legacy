#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

// Test 1: 8-Sector Direction Classifier & DirectInput mapping
static void test_sector_classification(void) {
    printf("--- [TEST 1] 8-Sector Directional Classification & Injection ---\n");
    mobile_input_init(1920, 1080);
    mobile_input_test_reset();

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
    mobile_input_init(1920, 1080);
    mobile_input_test_reset();

    // Default deadzone is 0.15 (15%), radius is ~90px
    // Touch inside deadzone: dx = 5px, dy = 5px from center (180, 900)
    float joy_cx = 180.0f;
    float joy_cy = 1080.0f - 180.0f;

    mobile_input_on_touch(0 /* DOWN */, joy_cx + 5.0f, joy_cy + 5.0f, 0);
    mobile_input_on_touch(2 /* MOVE */, joy_cx + 5.0f, joy_cy + 5.0f, 0);

    assert(s_mock_pad_lx == 0);
    assert(s_mock_pad_ly == 0);
    printf("  Sub-deadzone touch (5px): Axis=(%d, %d) -> PASS (Zero output)\n", s_mock_pad_lx, s_mock_pad_ly);

    // Touch outside deadzone: 60px right
    mobile_input_on_touch(2 /* MOVE */, joy_cx + 60.0f, joy_cy, 0);
    assert(s_mock_pad_lx > 10000);
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
    mobile_input_init(1920, 1080);
    mobile_input_test_reset();

    float joy_cx = 180.0f;
    float joy_cy = 1080.0f - 180.0f;

    // Pointer 0: Joystick touch (held down)
    mobile_input_on_touch(0 /* DOWN */, joy_cx + 70.0f, joy_cy, 0);
    assert(s_mock_pad_lx > 0);

    // Pointer 1: Tap LOFT button on right side (sw - 220, sh - 320)
    float loft_x = 1920.0f - 220.0f + 20.0f;
    float loft_y = 1080.0f - 220.0f - 80.0f + 20.0f;
    mobile_input_on_touch(5 /* POINTER_DOWN */, loft_x, loft_y, 1);
    assert(s_mock_dik_state[0x2A] == 1); // DIK_LSHIFT active (Loft enabled)
    printf("  Pointer 0 (Joystick) + Pointer 1 (LOFT Button) active concurrently: Stick LX=%d, DIK_LSHIFT=%d -> PASS\n",
           s_mock_pad_lx, s_mock_dik_state[0x2A]);

    // Pointer 2: Batting swipe in center of screen
    mobile_input_on_touch(5 /* POINTER_DOWN */, 960.0f, 540.0f, 2);
    mobile_input_on_touch(2 /* MOVE */, 960.0f, 440.0f, 2);
    mobile_input_on_touch(6 /* POINTER_UP */, 960.0f, 440.0f, 2);

    MobileInputTelemetry telem;
    mobile_input_get_telemetry(&telem);
    assert(telem.last_stroke.valid);
    assert(telem.last_stroke.sector == SECTOR_STRAIGHT_DRIVE);
    assert(telem.last_stroke.is_lofted); // Must inherit active loft state!
    printf("  Pointer 2 (Center Swipe): Stroke Sector=%d, Lofted=%d -> PASS\n",
           telem.last_stroke.sector, telem.last_stroke.is_lofted);

    // Release Pointer 0
    mobile_input_on_touch(1 /* UP */, joy_cx + 70.0f, joy_cy, 0);
    assert(s_mock_pad_lx == 0);
    printf("  --> PASS: Multi-touch isolation and concurrent processing verified!\n\n");
}

int main(void) {
    printf("==================================================================\n");
    printf("CRICKET LEGACY: Native Mobile Input System Unit & Integration Test\n");
    printf("==================================================================\n\n");

    test_sector_classification();
    test_joystick_deadzone();
    test_multitouch_concurrency();

    printf("==================================================================\n");
    printf("ALL MOBILE INPUT UNIT TESTS COMPLETED SUCCESSFULLY!\n");
    printf("==================================================================\n");
    return 0;
}
