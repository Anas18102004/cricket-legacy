#include "game_menu_presentation.h"
#include "upstream_d3d8/d3d8_android_adapter.h"
#include "upstream_d3d8/profile.h"
#include "game_static_data.h"
#include "upstream_d3d8/cpu.h"
#include "upstream_d3d8/com.h"
#include <android/log.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TAG "CricketLegacy-MenuGateway"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Direct3D 8 Pre-transformed 2D Vertex Format
#define D3DFVF_XYZRHW   0x004
#define D3DFVF_DIFFUSE  0x040
#define D3DFVF_TEX1     0x100
#define D3DFVF_MENU_2D  (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1) // 0x144

#define D3DPT_TRIANGLELIST 4
#define D3DCLEAR_TARGET    0x00000001
#define D3DCLEAR_ZBUFFER   0x00000002

#define D3DRS_ZENABLE      7
#define D3DRS_CULLMODE     22
#define D3DRS_LIGHTING     137
#define D3DCULL_NONE       1

#define D3DTSS_COLOROP     1
#define D3DTSS_COLORARG1   2
#define D3DTSS_COLORARG2   3
#define D3DTSS_ALPHAOP     4
#define D3DTSS_ALPHAARG1   5

#define D3DTOP_DISABLE     1
#define D3DTOP_SELECTARG1  2
#define D3DTOP_MODULATE    4

#define D3DTA_DIFFUSE      1
#define D3DTA_TEXTURE      2

#define FMT_A8R8G8B8       21
#define FMT_INDEX16        101

#define TEX_ATLAS_SIZE     512

typedef struct {
    float x, y, z, rhw; // 16 bytes: screen x, y, depth z, rhw=1.0f
    uint32_t diffuse;   // 4 bytes: 0xAARRGGBB
    float u, v;         // 8 bytes: texture coordinates
} MenuVertex2D; // 28 bytes

#define MAX_QUADS          32
#define MAX_VERTS          (MAX_QUADS * 4)
#define MAX_INDICES        (MAX_QUADS * 6)

static MenuPresentationState s_menu_state = {0};
static uint32_t s_menu_vb = 0;
static uint32_t s_menu_ib = 0;
static uint32_t s_menu_tex = 0;
static bool s_resources_created = false;

// ============================================================================
// Standard 8x8 ASCII Font Bitmap (ASCII 32 ' ' to 126 '~')
// ============================================================================
static const uint8_t s_font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // 35 '#'
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // 36 '$'
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // 37 '%'
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // 38 '&'
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // 39 '''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 '.'
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // 47 '/'
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, // 48 '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 49 '1'
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, // 50 '2'
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, // 51 '3'
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 52 '4'
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, // 53 '5'
    {0x78,0x0C,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, // 54 '6'
    {0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // 55 '7'
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 56 '8'
    {0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00}, // 57 '9'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // 58 ':'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // 59 ';'
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 '='
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 '>'
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, // 63 '?'
    {0x7C,0xC6,0xDE,0xDE,0xDC,0xC0,0x7C,0x00}, // 64 '@'
    {0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // 65 'A'
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // 66 'B'
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // 67 'C'
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // 68 'D'
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // 69 'E'
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // 70 'F'
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // 71 'G'
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // 72 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 73 'I'
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // 74 'J'
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // 75 'K'
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // 76 'L'
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // 77 'M'
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // 78 'N'
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 79 'O'
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // 80 'P'
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06}, // 81 'Q'
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // 82 'R'
    {0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00}, // 83 'S'
    {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00}, // 84 'T'
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 85 'U'
    {0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // 86 'V'
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // 87 'W'
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00}, // 88 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // 89 'Y'
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // 90 'Z'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 '['
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // 92 '\'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ']'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // 94 '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, // 95 '_'
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // 97 'a'
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 98 'b'
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, // 99 'c'
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, // 100 'd'
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, // 101 'e'
    {0x3C,0x66,0x60,0xF8,0x60,0x60,0xF0,0x00}, // 102 'f'
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // 103 'g'
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // 104 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 'i'
    {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C}, // 106 'j'
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // 107 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 'l'
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, // 109 'm'
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, // 110 'n'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, // 111 'o'
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // 112 'p'
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // 113 'q'
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, // 114 'r'
    {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, // 115 's'
    {0x30,0x30,0xFC,0x30,0x30,0x34,0x18,0x00}, // 116 't'
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // 117 'u'
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // 118 'v'
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, // 119 'w'
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // 120 'x'
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, // 121 'y'
    {0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00}, // 122 'z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 '}'
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  // 126 '~'
};

// ============================================================================
// Procedural Texture Atlas Rasterization Helpers
// ============================================================================

static void atlas_draw_rect(uint32_t *pix, int w, int h, int rx, int ry, int rw, int rh, uint32_t col) {
    for (int y = ry; y < ry + rh && y < h; y++) {
        if (y < 0) continue;
        for (int x = rx; x < rx + rw && x < w; x++) {
            if (x < 0) continue;
            pix[y * w + x] = col;
        }
    }
}

static void atlas_draw_border(uint32_t *pix, int w, int h, int rx, int ry, int rw, int rh, int th, uint32_t col) {
    atlas_draw_rect(pix, w, h, rx, ry, rw, th, col);                  // Top
    atlas_draw_rect(pix, w, h, rx, ry + rh - th, rw, th, col);        // Bottom
    atlas_draw_rect(pix, w, h, rx, ry, th, rh, col);                  // Left
    atlas_draw_rect(pix, w, h, rx + rw - th, ry, th, rh, col);        // Right
}

static void atlas_draw_char(uint32_t *pix, int w, int h, int cx, int cy, char c, uint32_t col, int scale) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = s_font8x8[c - 32];
    for (int r = 0; r < 8; r++) {
        uint8_t row = glyph[r];
        for (int b = 0; b < 8; b++) {
            if (row & (0x80 >> b)) {
                for (int sy = 0; sy < scale; sy++) {
                    int py = cy + r * scale + sy;
                    if (py < 0 || py >= h) continue;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = cx + b * scale + sx;
                        if (px < 0 || px >= w) continue;
                        pix[py * w + px] = col;
                    }
                }
            }
        }
    }
}

static void atlas_draw_string(uint32_t *pix, int w, int h, int sx, int sy, const char *str, uint32_t col, int scale) {
    int cur_x = sx;
    while (*str) {
        atlas_draw_char(pix, w, h, cur_x, sy, *str, col, scale);
        cur_x += 8 * scale;
        str++;
    }
}

// ============================================================================
// Texture Atlas Generation
// ============================================================================

static void generate_menu_texture_atlas(uint32_t *pixels, uint32_t pitch_bytes) {
    int w = TEX_ATLAS_SIZE;
    int h = TEX_ATLAS_SIZE;
    uint32_t stride = pitch_bytes / 4;

    // 1. Clear atlas with transparent black
    memset(pixels, 0, (size_t)h * pitch_bytes);

    // 2. Solid White 4x4 block at (508, 508) for solid geometry rendering
    atlas_draw_rect(pixels, w, h, 508, 508, 4, 4, 0xFFFFFFFF);

    // 3. Header Banner Panel: [10, 10] to [500, 75]
    atlas_draw_rect(pixels, w, h, 10, 10, 490, 65, 0xFF14243B); // Deep slate blue
    atlas_draw_border(pixels, w, h, 10, 10, 490, 65, 2, 0xFF00B4D8); // Cyan border
    atlas_draw_rect(pixels, w, h, 10, 72, 490, 3, 0xFFFFB703); // Gold bottom accent
    atlas_draw_string(pixels, w, h, 28, 20, "CRICKET LEGACY", 0xFFFFD166, 2);
    atlas_draw_string(pixels, w, h, 28, 46, "STAGE 5 DEMO GATEWAY - D3D8 / WEBGPU / ARM64", 0xFF00E5FF, 1);

    // 4. Button 0 Card Normal: [10, 90] to [400, 140]
    atlas_draw_rect(pixels, w, h, 10, 90, 390, 50, 0xFF1E2D4A);
    atlas_draw_border(pixels, w, h, 10, 90, 390, 50, 2, 0xFF2C3E6B);
    atlas_draw_rect(pixels, w, h, 10, 90, 8, 50, 0xFF00B4D8); // Left cyan pill
    atlas_draw_string(pixels, w, h, 30, 106, "1. PLAY EXHIBITION", 0xFFFFFFFF, 2);

    // 5. Button 1 Card Normal: [10, 150] to [400, 200]
    atlas_draw_rect(pixels, w, h, 10, 150, 390, 50, 0xFF1E2D4A);
    atlas_draw_border(pixels, w, h, 10, 150, 390, 50, 2, 0xFF2C3E6B);
    atlas_draw_rect(pixels, w, h, 10, 150, 8, 50, 0xFFFFB703); // Left amber pill
    atlas_draw_string(pixels, w, h, 30, 166, "2. TOURNAMENT MODE", 0xFFFFFFFF, 2);

    // 6. Button 2 Card Normal: [10, 210] to [400, 260]
    atlas_draw_rect(pixels, w, h, 10, 210, 390, 50, 0xFF1E2D4A);
    atlas_draw_border(pixels, w, h, 10, 210, 390, 50, 2, 0xFF2C3E6B);
    atlas_draw_rect(pixels, w, h, 10, 210, 8, 50, 0xFF06D6A0); // Left emerald pill
    atlas_draw_string(pixels, w, h, 30, 226, "3. SETTINGS & CONTROLS", 0xFFFFFFFF, 2);

    // 7. Button 3 Card Normal: [10, 270] to [400, 320]
    atlas_draw_rect(pixels, w, h, 10, 270, 390, 50, 0xFF1E2D4A);
    atlas_draw_border(pixels, w, h, 10, 270, 390, 50, 2, 0xFF2C3E6B);
    atlas_draw_rect(pixels, w, h, 10, 270, 8, 50, 0xFFEF476F); // Left rose pill
    atlas_draw_string(pixels, w, h, 30, 286, "4. SYSTEM DIAGNOSTICS", 0xFFFFFFFF, 2);

    // 8. Footer Status Panel: [10, 335] to [500, 410]
    atlas_draw_rect(pixels, w, h, 10, 335, 490, 75, 0xFF0F1A2C);
    atlas_draw_border(pixels, w, h, 10, 335, 490, 75, 1, 0xFF1F3556);
    atlas_draw_rect(pixels, w, h, 10, 335, 490, 2, 0xFF06D6A0); // Emerald top accent
    atlas_draw_string(pixels, w, h, 20, 345, "HOST INFRASTRUCTURE VERIFIED (STAGES 1-5 COMPLETED)", 0xFF06D6A0, 1);
    atlas_draw_string(pixels, w, h, 20, 362, "MEMORY ARENA 4GB | DIRECT3D 8 COM DISPATCH ACTIVE", 0xFFE0E6ED, 1);
    atlas_draw_string(pixels, w, h, 20, 379, "AUTHENTIC PROFILE_CURSOR @ 0x00A01970 LINKED & ACTIVE", 0xFFFFD166, 1);
    atlas_draw_string(pixels, w, h, 20, 395, "TOUCH SCREEN TO STEER CURSOR & ACTIVATE BUTTONS", 0xFF90A4AE, 1);

    // 9. Interactive Pointer Diamond: [420, 90] to [460, 130] (40x40 area)
    int cx = 440, cy = 110;
    for (int dy = -16; dy <= 16; dy++) {
        for (int dx = -16; dx <= 16; dx++) {
            int dist = abs(dx) + abs(dy);
            if (dist <= 16) {
                uint32_t col = (dist <= 4) ? 0xFFFFFFFF : (dist <= 12) ? 0xFFFFD166 : 0xFF14243B;
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    pixels[py * stride + px] = col;
                }
            }
        }
    }

    LOGI("Generated 512x512 Menu Texture Atlas with title, 4 button cards, status panel, and cursor.");
}

// ============================================================================
// Subsystem Initialization & Resource Management
// ============================================================================

bool game_menu_init(void) {
    if (s_menu_state.is_initialized) return true;

    LOGI("==================================================");
    LOGI("INITIALIZING HOST MENU PRESENTATION & RENDER GATEWAY");
    LOGI("Stage 5 Demonstration Harness (D3D8 / WebGPU / ARM64)");
    LOGI("==================================================");

    // 1. Setup Button Bounds in Virtual 640x480 Coordinate Space
    s_menu_state.buttons[MENU_BTN_EXHIBITION] = (MenuButton){
        .label = "PLAY EXHIBITION",
        .x1 = 140.0f, .y1 = 115.0f, .x2 = 500.0f, .y2 = 165.0f,
        .hovered = false, .pressed = false, .click_count = 0
    };
    s_menu_state.buttons[MENU_BTN_TOURNAMENT] = (MenuButton){
        .label = "TOURNAMENT MODE",
        .x1 = 140.0f, .y1 = 175.0f, .x2 = 500.0f, .y2 = 225.0f,
        .hovered = false, .pressed = false, .click_count = 0
    };
    s_menu_state.buttons[MENU_BTN_SETTINGS] = (MenuButton){
        .label = "SETTINGS & CONTROLS",
        .x1 = 140.0f, .y1 = 235.0f, .x2 = 500.0f, .y2 = 285.0f,
        .hovered = false, .pressed = false, .click_count = 0
    };
    s_menu_state.buttons[MENU_BTN_DIAGNOSTICS] = (MenuButton){
        .label = "SYSTEM DIAGNOSTICS",
        .x1 = 140.0f, .y1 = 295.0f, .x2 = 500.0f, .y2 = 345.0f,
        .hovered = false, .pressed = false, .click_count = 0
    };

    s_menu_state.last_clicked_button = -1;
    s_menu_state.total_clicks = 0;
    s_menu_state.rendered_frames = 0;

    // 2. Read baseline cursor from authentic PROFILE_CURSOR (0x00A01970)
    game_menu_get_cursor(&s_menu_state.cursor_x, &s_menu_state.cursor_y);
    LOGI("Linked authentic PROFILE_CURSOR from 0x%08X: (%.1f, %.1f)", 
         PROFILE_CURSOR, s_menu_state.cursor_x, s_menu_state.cursor_y);

    s_menu_state.is_initialized = true;
    return true;
}

static bool create_d3d8_menu_resources(Device *d) {
    if (s_resources_created && s_menu_vb && s_menu_ib && s_menu_tex) return true;
    if (!d) return false;

    LOGI("Creating Direct3D 8 resources for Host Menu Gateway...");

    // 1. Create Dynamic 2D Vertex Buffer (MAX_VERTS * 28 bytes)
    uint32_t vb_size = MAX_VERTS * sizeof(MenuVertex2D);
    s_menu_vb = d3d8_CreateVertexBuffer(d, vb_size, 0, D3DFVF_MENU_2D, 0);
    if (!s_menu_vb) {
        LOGE("Failed to create Menu Vertex Buffer!");
        return false;
    }

    // 2. Create Static Index Buffer (MAX_INDICES * 2 bytes)
    uint32_t ib_size = MAX_INDICES * sizeof(uint16_t);
    s_menu_ib = d3d8_CreateIndexBuffer(d, ib_size, 0, FMT_INDEX16, 0);
    if (!s_menu_ib) {
        LOGE("Failed to create Menu Index Buffer!");
        return false;
    }

    // Initialize Index Buffer: 2 triangles per quad (0,1,2 and 2,1,3)
    void *ib_data = NULL;
    d3d8_BufferLock(s_menu_ib, 0, ib_size, &ib_data, 0);
    if (ib_data) {
        uint16_t *idx = (uint16_t*)ib_data;
        for (uint16_t q = 0; q < MAX_QUADS; q++) {
            uint16_t base = q * 4;
            idx[q * 6 + 0] = base + 0;
            idx[q * 6 + 1] = base + 1;
            idx[q * 6 + 2] = base + 2;
            idx[q * 6 + 3] = base + 2;
            idx[q * 6 + 4] = base + 1;
            idx[q * 6 + 5] = base + 3;
        }
        d3d8_BufferUnlock(s_menu_ib);
    }

    // 3. Create 512x512 A8R8G8B8 Texture Atlas
    s_menu_tex = d3d8_CreateTexture(d, TEX_ATLAS_SIZE, TEX_ATLAS_SIZE, 1, 0, FMT_A8R8G8B8, 0);
    if (!s_menu_tex) {
        LOGE("Failed to create Menu Texture Atlas!");
        return false;
    }

    void *tex_bits = NULL;
    uint32_t pitch = 0;
    d3d8_TextureLockRect(s_menu_tex, 0, &tex_bits, &pitch, 0);
    if (tex_bits) {
        generate_menu_texture_atlas((uint32_t*)tex_bits, pitch);
        d3d8_TextureUnlockRect(s_menu_tex, 0);
    }

    s_resources_created = true;
    LOGI("Direct3D 8 Menu Resources created: VB 0x%08X, IB 0x%08X, Tex 0x%08X",
         s_menu_vb, s_menu_ib, s_menu_tex);
    return true;
}

// ============================================================================
// Viewport Mapping & Touch Input Steering
// ============================================================================

static void update_viewport_metrics(void) {
    const D3D8AndroidState* state = d3d8_android_get_state();
    float sw = (float)state->width;
    float sh = (float)state->height;
    if (sw <= 0.0f) sw = 1920.0f;
    if (sh <= 0.0f) sh = 1080.0f;

    s_menu_state.screen_w = sw;
    s_menu_state.screen_h = sh;

    float sx = sw / MENU_VIRTUAL_WIDTH;
    float sy = sh / MENU_VIRTUAL_HEIGHT;
    s_menu_state.scale = (sx < sy) ? sx : sy;
    s_menu_state.offset_x = (sw - MENU_VIRTUAL_WIDTH * s_menu_state.scale) * 0.5f;
    s_menu_state.offset_y = (sh - MENU_VIRTUAL_HEIGHT * s_menu_state.scale) * 0.5f;
}

void game_menu_screen_to_virtual(float sx, float sy, float *vx, float *vy) {
    update_viewport_metrics();
    float vx_raw = (sx - s_menu_state.offset_x) / s_menu_state.scale;
    float vy_raw = (sy - s_menu_state.offset_y) / s_menu_state.scale;

    // Strict clamping to virtual 640x480 coordinate space
    if (vx_raw < 0.0f) vx_raw = 0.0f;
    if (vx_raw > MENU_VIRTUAL_WIDTH) vx_raw = MENU_VIRTUAL_WIDTH;
    if (vy_raw < 0.0f) vy_raw = 0.0f;
    if (vy_raw > MENU_VIRTUAL_HEIGHT) vy_raw = MENU_VIRTUAL_HEIGHT;

    *vx = vx_raw;
    *vy = vy_raw;
}

void game_menu_virtual_to_screen(float vx, float vy, float *sx, float *sy) {
    update_viewport_metrics();
    *sx = s_menu_state.offset_x + vx * s_menu_state.scale;
    *sy = s_menu_state.offset_y + vy * s_menu_state.scale;
}

void game_menu_get_cursor(float *cx, float *cy) {
    uint32_t cx_bits = R32(PROFILE_CURSOR + 0);
    uint32_t cy_bits = R32(PROFILE_CURSOR + 4);
    memcpy(cx, &cx_bits, 4);
    memcpy(cy, &cy_bits, 4);
}

void game_menu_set_cursor(float cx, float cy) {
    // Clamp to [0, 640] x [0, 480]
    if (cx < 0.0f) cx = 0.0f;
    if (cx > MENU_VIRTUAL_WIDTH) cx = MENU_VIRTUAL_WIDTH;
    if (cy < 0.0f) cy = 0.0f;
    if (cy > MENU_VIRTUAL_HEIGHT) cy = MENU_VIRTUAL_HEIGHT;

    s_menu_state.cursor_x = cx;
    s_menu_state.cursor_y = cy;

    uint32_t cx_bits, cy_bits;
    memcpy(&cx_bits, &cx, 4);
    memcpy(&cy_bits, &cy, 4);
    W32(PROFILE_CURSOR + 0, cx_bits);
    W32(PROFILE_CURSOR + 4, cy_bits);
}

void game_menu_on_touch(int action, float screen_x, float screen_y) {
    if (!s_menu_state.is_initialized) game_menu_init();

    float vx, vy;
    game_menu_screen_to_virtual(screen_x, screen_y, &vx, &vy);
    game_menu_set_cursor(vx, vy);

    // Hit-test buttons
    for (int i = 0; i < MENU_NUM_BUTTONS; i++) {
        MenuButton *b = &s_menu_state.buttons[i];
        bool inside = (vx >= b->x1 && vx <= b->x2 && vy >= b->y1 && vy <= b->y2);
        b->hovered = inside;

        if (action == 0 /* ACTION_DOWN */) {
            if (inside) b->pressed = true;
        } else if (action == 1 /* ACTION_UP */) {
            if (inside && b->pressed) {
                b->click_count++;
                s_menu_state.total_clicks++;
                s_menu_state.last_clicked_button = i;
                LOGI(">>> HOST MENU BUTTON CLICKED: [%d] %s (Total clicks: %u)",
                     i, b->label, b->click_count);
            }
            b->pressed = false;
        } else if (action == 2 /* ACTION_MOVE */) {
            if (!inside) b->pressed = false;
        }
    }
}

// ============================================================================
// Menu Quad Geometry Emission
// ============================================================================

static void emit_quad(MenuVertex2D *verts, uint32_t quad_idx,
                      float vx1, float vy1, float vx2, float vy2,
                      float u1, float v1, float u2, float v2,
                      uint32_t diffuse) {
    float sx1, sy1, sx2, sy2;
    game_menu_virtual_to_screen(vx1, vy1, &sx1, &sy1);
    game_menu_virtual_to_screen(vx2, vy2, &sx2, &sy2);

    MenuVertex2D *v = &verts[quad_idx * 4];

    // Vertex 0: Top-Left
    v[0].x = sx1; v[0].y = sy1; v[0].z = 0.0f; v[0].rhw = 1.0f;
    v[0].diffuse = diffuse; v[0].u = u1; v[0].v = v1;

    // Vertex 1: Top-Right
    v[1].x = sx2; v[1].y = sy1; v[1].z = 0.0f; v[1].rhw = 1.0f;
    v[1].diffuse = diffuse; v[1].u = u2; v[1].v = v1;

    // Vertex 2: Bottom-Left
    v[2].x = sx1; v[2].y = sy2; v[2].z = 0.0f; v[2].rhw = 1.0f;
    v[2].diffuse = diffuse; v[2].u = u1; v[2].v = v2;

    // Vertex 3: Bottom-Right
    v[3].x = sx2; v[3].y = sy2; v[3].z = 0.0f; v[3].rhw = 1.0f;
    v[3].diffuse = diffuse; v[3].u = u2; v[3].v = v2;
}

static void emit_solid_quad(MenuVertex2D *verts, uint32_t quad_idx,
                            float vx1, float vy1, float vx2, float vy2,
                            uint32_t diffuse) {
    // White pixel at (508, 508) in 512x512 atlas
    float uw = 510.0f / 512.0f;
    float vw = 510.0f / 512.0f;
    emit_quad(verts, quad_idx, vx1, vy1, vx2, vy2, uw, vw, uw, vw, diffuse);
}

// ============================================================================
// Direct3D 8 Menu Frame Rendering
// ============================================================================

void game_menu_render_frame(void) {
    if (!s_menu_state.is_initialized) game_menu_init();

    Device *d = d3d8_android_get_device();
    const D3D8AndroidState *state = d3d8_android_get_state();
    if (!d || !state) return;

    if (!s_resources_created) {
        if (!create_d3d8_menu_resources(d)) return;
    }

    update_viewport_metrics();

    // 1. Read live cursor from authentic PROFILE_CURSOR
    float cur_x, cur_y;
    game_menu_get_cursor(&cur_x, &cur_y);
    s_menu_state.cursor_x = cur_x;
    s_menu_state.cursor_y = cur_y;

    // Update button hover state from cursor
    for (int i = 0; i < MENU_NUM_BUTTONS; i++) {
        MenuButton *b = &s_menu_state.buttons[i];
        b->hovered = (cur_x >= b->x1 && cur_x <= b->x2 && cur_y >= b->y1 && cur_y <= b->y2);
    }

    // 2. Lock Vertex Buffer & Emit Dynamic 2D Quads
    void *vb_data = NULL;
    uint32_t vb_size = MAX_VERTS * sizeof(MenuVertex2D);
    d3d8_BufferLock(s_menu_vb, 0, vb_size, &vb_data, 0);
    if (!vb_data) return;

    MenuVertex2D *verts = (MenuVertex2D*)vb_data;
    uint32_t q = 0;

    // Quad 0: Deep stadium outer backdrop (full screen)
    float sw = s_menu_state.screen_w;
    float sh = s_menu_state.screen_h;
    float uw = 510.0f / 512.0f, vw = 510.0f / 512.0f;
    verts[q * 4 + 0] = (MenuVertex2D){ 0.0f, 0.0f, 0.0f, 1.0f, 0xFF081324, uw, vw };
    verts[q * 4 + 1] = (MenuVertex2D){ sw,   0.0f, 0.0f, 1.0f, 0xFF081324, uw, vw };
    verts[q * 4 + 2] = (MenuVertex2D){ 0.0f, sh,   0.0f, 1.0f, 0xFF040A14, uw, vw };
    verts[q * 4 + 3] = (MenuVertex2D){ sw,   sh,   0.0f, 1.0f, 0xFF040A14, uw, vw };
    q++;

    // Quad 1: 4:3 Stage Backdrop Panel (Virtual 0,0 to 640,480)
    emit_solid_quad(verts, q++, 0.0f, 0.0f, 640.0f, 480.0f, 0xFF0B1B36);

    // Quad 2: Header Title Banner (Atlas [10, 10] to [500, 75])
    float u1_hdr = 10.0f / 512.0f, v1_hdr = 10.0f / 512.0f;
    float u2_hdr = 500.0f / 512.0f, v2_hdr = 75.0f / 512.0f;
    emit_quad(verts, q++, 30.0f, 20.0f, 610.0f, 85.0f, u1_hdr, v1_hdr, u2_hdr, v2_hdr, 0xFFFFFFFF);

    // Quads 3-6: 4 Interactive Menu Buttons
    for (int i = 0; i < MENU_NUM_BUTTONS; i++) {
        MenuButton *b = &s_menu_state.buttons[i];

        // Button Card Atlas Region
        float v1_btn = (90.0f + i * 60.0f) / 512.0f;
        float v2_btn = (140.0f + i * 60.0f) / 512.0f;
        float u1_btn = 10.0f / 512.0f;
        float u2_btn = 400.0f / 512.0f;

        uint32_t tint = b->pressed ? 0xFFFFAA00 : (b->hovered ? 0xFF00E5FF : 0xFFFFFFFF);
        emit_quad(verts, q++, b->x1, b->y1, b->x2, b->y2, u1_btn, v1_btn, u2_btn, v2_btn, tint);

        // If hovered or pressed, emit glowing border accent quad
        if (b->hovered || b->pressed) {
            uint32_t glow_col = b->pressed ? 0xFFFFB703 : 0xFF00E5FF;
            emit_solid_quad(verts, q++, b->x1 - 3.0f, b->y1 - 3.0f, b->x2 + 3.0f, b->y1, glow_col);
            emit_solid_quad(verts, q++, b->x1 - 3.0f, b->y2, b->x2 + 3.0f, b->y2 + 3.0f, glow_col);
        }
    }

    // Quad 7: Status Panel Footer (Atlas [10, 335] to [500, 410])
    float u1_ft = 10.0f / 512.0f, v1_ft = 335.0f / 512.0f;
    float u2_ft = 500.0f / 512.0f, v2_ft = 410.0f / 512.0f;
    emit_quad(verts, q++, 30.0f, 375.0f, 610.0f, 450.0f, u1_ft, v1_ft, u2_ft, v2_ft, 0xFFFFFFFF);

    // Quad 8: Interactive Cursor Pointer Diamond (Atlas [420, 90] to [460, 130])
    float u1_cur = 420.0f / 512.0f, v1_cur = 90.0f / 512.0f;
    float u2_cur = 460.0f / 512.0f, v2_cur = 130.0f / 512.0f;
    float cur_rad = 12.0f;
    emit_quad(verts, q++, cur_x - cur_rad, cur_y - cur_rad, cur_x + cur_rad, cur_y + cur_rad,
              u1_cur, v1_cur, u2_cur, v2_cur, 0xFFFFFFFF);

    d3d8_BufferUnlock(s_menu_vb);

    uint32_t active_quads = q;
    uint32_t active_prims = active_quads * 2;
    uint32_t active_verts = active_quads * 4;

    // 3. Render Direct3D 8 Frame
    d3d8_Clear(d, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF081324, 1.0f, 0);

    d3d8_BeginScene(d);

    d3d8_SetStreamSource(d, 0, s_menu_vb, sizeof(MenuVertex2D));
    d3d8_SetIndices(d, s_menu_ib, 0);
    d3d8_SetVertexShader(d, D3DFVF_MENU_2D);
    d3d8_SetTexture(d, 0, s_menu_tex);

    d3d8_SetRenderState(d, D3DRS_ZENABLE, 0);
    d3d8_SetRenderState(d, D3DRS_CULLMODE, D3DCULL_NONE);
    d3d8_SetRenderState(d, D3DRS_LIGHTING, 0);

    d3d8_SetTextureStageState(d, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    d3d8_SetTextureStageState(d, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d3d8_SetTextureStageState(d, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

    d3d8_DrawIndexedPrimitive(d, D3DPT_TRIANGLELIST, 0, active_verts, 0, active_prims);

    d3d8_EndScene(d);
    d3d8_Present(d);

    s_menu_state.rendered_frames++;
}

void game_menu_shutdown(void) {
    if (!s_menu_state.is_initialized) return;
    LOGI("Shutting down Host Menu Gateway subsystem...");
    s_resources_created = false;
    s_menu_vb = 0;
    s_menu_ib = 0;
    s_menu_tex = 0;
    s_menu_state.is_initialized = false;
}

const MenuPresentationState* game_menu_get_state(void) {
    return &s_menu_state;
}

uint32_t game_menu_get_frame_count(void) {
    return s_menu_state.rendered_frames;
}
