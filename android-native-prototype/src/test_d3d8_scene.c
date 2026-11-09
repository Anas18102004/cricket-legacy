#include "test_d3d8_scene.h"
#include "upstream_d3d8/d3d8_android_adapter.h"
#include <android/log.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TAG "CricketLegacy-D3D8Scene"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Direct3D 8 constants
#define D3DFVF_XYZ       0x002
#define D3DFVF_DIFFUSE   0x040
#define D3DFVF_TEX1      0x100
#define D3DFVF_CUSTOM    (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1) // 0x142

#define D3DPT_TRIANGLELIST 4

#define D3DCLEAR_TARGET  0x00000001
#define D3DCLEAR_ZBUFFER 0x00000002

#define D3DTS_VIEW       2
#define D3DTS_PROJECTION 3
#define D3DTS_WORLD      256

#define D3DRS_ZENABLE       7
#define D3DRS_CULLMODE      22
#define D3DRS_ZWRITEENABLE  14
#define D3DRS_ZFUNC         23
#define D3DRS_LIGHTING      137

#define D3DCULL_NONE 1
#define D3DCULL_CW   2
#define D3DCULL_CCW  3

#define D3DCMP_LESSEQUAL 4

#define D3DTSS_COLOROP   1
#define D3DTSS_COLORARG1 2
#define D3DTSS_COLORARG2 3
#define D3DTSS_ALPHAOP   4
#define D3DTSS_ALPHAARG1 5

#define D3DTOP_DISABLE  1
#define D3DTOP_SELECTARG1 2
#define D3DTOP_MODULATE 4

#define D3DTA_DIFFUSE 1
#define D3DTA_TEXTURE 2

#define FMT_A8R8G8B8 21
#define FMT_INDEX16  101

typedef struct {
    float x, y, z;
    uint32_t diffuse;
    float u, v;
} D3D8Vertex;

static uint32_t s_vb = 0;
static uint32_t s_ib = 0;
static uint32_t s_tex = 0;
static bool s_scene_initialized = false;
static float s_rot_y = 0.0f;
static uint32_t s_rendered_frames = 0;

// ============================================================================
// Matrix Math (Row-vector order for Direct3D 8: v * W * V * P)
// ============================================================================

static void mat4_identity(float m[16]) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_mul(const float a[16], const float b[16], float r[16]) {
    float out[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) {
                s += a[i * 4 + k] * b[k * 4 + j];
            }
            out[i * 4 + j] = s;
        }
    }
    memcpy(r, out, 64);
}

static void mat4_perspective(float m[16], float fov_rad, float aspect, float znear, float zfar) {
    memset(m, 0, 64);
    float h = 1.0f / tanf(fov_rad * 0.5f);
    float w = h / aspect;
    float q = zfar / (zfar - znear);

    m[0] = w;
    m[5] = h;
    m[10] = q;
    m[11] = 1.0f;
    m[14] = -q * znear;
    m[15] = 0.0f;
}

static void mat4_lookat(float m[16], const float eye[3], const float target[3], const float up[3]) {
    float zaxis[3] = { target[0] - eye[0], target[1] - eye[1], target[2] - eye[2] };
    float len = sqrtf(zaxis[0]*zaxis[0] + zaxis[1]*zaxis[1] + zaxis[2]*zaxis[2]);
    zaxis[0] /= len; zaxis[1] /= len; zaxis[2] /= len;

    float xaxis[3] = {
        up[1] * zaxis[2] - up[2] * zaxis[1],
        up[2] * zaxis[0] - up[0] * zaxis[2],
        up[0] * zaxis[1] - up[1] * zaxis[0]
    };
    len = sqrtf(xaxis[0]*xaxis[0] + xaxis[1]*xaxis[1] + xaxis[2]*xaxis[2]);
    xaxis[0] /= len; xaxis[1] /= len; xaxis[2] /= len;

    float yaxis[3] = {
        zaxis[1] * xaxis[2] - zaxis[2] * xaxis[1],
        zaxis[2] * xaxis[0] - zaxis[0] * xaxis[2],
        zaxis[0] * xaxis[1] - zaxis[1] * xaxis[0]
    };

    memset(m, 0, 64);
    m[0] = xaxis[0]; m[1] = yaxis[0]; m[2] = zaxis[0];
    m[4] = xaxis[1]; m[5] = yaxis[1]; m[6] = zaxis[1];
    m[8] = xaxis[2]; m[9] = yaxis[2]; m[10] = zaxis[2];
    m[12] = -(xaxis[0]*eye[0] + xaxis[1]*eye[1] + xaxis[2]*eye[2]);
    m[13] = -(yaxis[0]*eye[0] + yaxis[1]*eye[1] + yaxis[2]*eye[2]);
    m[14] = -(zaxis[0]*eye[0] + zaxis[1]*eye[1] + zaxis[2]*eye[2]);
    m[15] = 1.0f;
}

static void mat4_rotate_y(float m[16], float rad) {
    mat4_identity(m);
    float c = cosf(rad);
    float s = sinf(rad);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

static void mat4_rotate_x(float m[16], float rad) {
    mat4_identity(m);
    float c = cosf(rad);
    float s = sinf(rad);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

// ============================================================================
// Scene Setup
// ============================================================================

bool test_d3d8_scene_init(void) {
    Device *d = d3d8_android_get_device();
    if (!d) {
        LOGE("test_d3d8_scene_init: D3D8 Device is NULL");
        return false;
    }

    LOGI("test_d3d8_scene_init: Initializing 3D cricket validation scene...");

    // 1. Create Vertex Buffer: 24 vertices for cricket pitch marker + 4 for stadium turf plane = 28 vertices
    uint32_t vb_size = 28 * sizeof(D3D8Vertex);
    s_vb = d3d8_CreateVertexBuffer(d, vb_size, 0, D3DFVF_CUSTOM, 0);
    if (!s_vb) {
        LOGE("Failed to create D3D8 Vertex Buffer");
        return false;
    }

    void *vb_data = NULL;
    d3d8_BufferLock(s_vb, 0, vb_size, &vb_data, 0);
    if (!vb_data) {
        LOGE("Failed to lock D3D8 Vertex Buffer");
        return false;
    }

    D3D8Vertex *verts = (D3D8Vertex*)vb_data;

    // Object 1: 3D Cricket Pitch Marker Box (Width 0.8, Height 0.25, Length 2.0)
    float w = 0.8f, h = 0.25f, l = 2.0f;
    uint32_t col_pitch = 0xFFFFFFFF;
    uint32_t col_side  = 0xFFCCCCCC;

    // Top Face (+Y)
    verts[0]  = (D3D8Vertex){ -w,  h, -l, col_pitch, 0.0f, 0.0f };
    verts[1]  = (D3D8Vertex){  w,  h, -l, col_pitch, 1.0f, 0.0f };
    verts[2]  = (D3D8Vertex){  w,  h,  l, col_pitch, 1.0f, 1.0f };
    verts[3]  = (D3D8Vertex){ -w,  h,  l, col_pitch, 0.0f, 1.0f };

    // Bottom Face (-Y)
    verts[4]  = (D3D8Vertex){ -w, -h,  l, col_side,  0.0f, 0.0f };
    verts[5]  = (D3D8Vertex){  w, -h,  l, col_side,  1.0f, 0.0f };
    verts[6]  = (D3D8Vertex){  w, -h, -l, col_side,  1.0f, 1.0f };
    verts[7]  = (D3D8Vertex){ -w, -h, -l, col_side,  0.0f, 1.0f };

    // Front Face (+Z)
    verts[8]  = (D3D8Vertex){ -w, -h,  l, col_side,  0.0f, 1.0f };
    verts[9]  = (D3D8Vertex){  w, -h,  l, col_side,  1.0f, 1.0f };
    verts[10] = (D3D8Vertex){  w,  h,  l, col_side,  1.0f, 0.0f };
    verts[11] = (D3D8Vertex){ -w,  h,  l, col_side,  0.0f, 0.0f };

    // Back Face (-Z)
    verts[12] = (D3D8Vertex){  w, -h, -l, col_side,  0.0f, 1.0f };
    verts[13] = (D3D8Vertex){ -w, -h, -l, col_side,  1.0f, 1.0f };
    verts[14] = (D3D8Vertex){ -w,  h, -l, col_side,  1.0f, 0.0f };
    verts[15] = (D3D8Vertex){  w,  h, -l, col_side,  0.0f, 0.0f };

    // Left Face (-X)
    verts[16] = (D3D8Vertex){ -w, -h, -l, col_side,  0.0f, 1.0f };
    verts[17] = (D3D8Vertex){ -w, -h,  l, col_side,  1.0f, 1.0f };
    verts[18] = (D3D8Vertex){ -w,  h,  l, col_side,  1.0f, 0.0f };
    verts[19] = (D3D8Vertex){ -w,  h, -l, col_side,  0.0f, 0.0f };

    // Right Face (+X)
    verts[20] = (D3D8Vertex){  w, -h,  l, col_side,  0.0f, 1.0f };
    verts[21] = (D3D8Vertex){  w, -h, -l, col_side,  1.0f, 1.0f };
    verts[22] = (D3D8Vertex){  w,  h, -l, col_side,  1.0f, 0.0f };
    verts[23] = (D3D8Vertex){  w,  h,  l, col_side,  0.0f, 0.0f };

    // Object 2: 3D Underlying Stadium Turf Ground Plane (Y = -1.0, Width 14, Length 14)
    float gw = 14.0f, gl = 14.0f, gy = -1.0f;
    uint32_t col_turf = 0xFF2E7D32; // Rich stadium turf green
    verts[24] = (D3D8Vertex){ -gw, gy, -gl, col_turf, 0.0f, 0.0f };
    verts[25] = (D3D8Vertex){  gw, gy, -gl, col_turf, 8.0f, 0.0f };
    verts[26] = (D3D8Vertex){  gw, gy,  gl, col_turf, 8.0f, 8.0f };
    verts[27] = (D3D8Vertex){ -gw, gy,  gl, col_turf, 0.0f, 8.0f };

    d3d8_BufferUnlock(s_vb);

    // 2. Create Index Buffer: 36 indices for box (12 triangles) + 6 for ground quad (2 triangles) = 42 indices
    uint32_t ib_size = 42 * sizeof(uint16_t);
    s_ib = d3d8_CreateIndexBuffer(d, ib_size, 0, FMT_INDEX16, 0);
    if (!s_ib) {
        LOGE("Failed to create D3D8 Index Buffer");
        return false;
    }

    void *ib_data = NULL;
    d3d8_BufferLock(s_ib, 0, ib_size, &ib_data, 0);
    if (!ib_data) {
        LOGE("Failed to lock D3D8 Index Buffer");
        return false;
    }

    uint16_t *indices = (uint16_t*)ib_data;
    // Box indices (6 faces * 2 triangles)
    for (int f = 0; f < 6; f++) {
        uint16_t base = (uint16_t)(f * 4);
        indices[f * 6 + 0] = base + 0;
        indices[f * 6 + 1] = base + 1;
        indices[f * 6 + 2] = base + 2;
        indices[f * 6 + 3] = base + 0;
        indices[f * 6 + 4] = base + 2;
        indices[f * 6 + 5] = base + 3;
    }

    // Ground plane indices (offset 36)
    indices[36] = 24;
    indices[37] = 25;
    indices[38] = 26;
    indices[39] = 24;
    indices[40] = 26;
    indices[41] = 27;

    d3d8_BufferUnlock(s_ib);

    // 3. Create 64x64 Procedural Cricket Pitch / Turf Texture
    s_tex = d3d8_CreateTexture(d, 64, 64, 1, 0, FMT_A8R8G8B8, 0);
    if (!s_tex) {
        LOGE("Failed to create D3D8 Texture");
        return false;
    }

    void *tex_bits = NULL;
    uint32_t pitch = 0;
    d3d8_TextureLockRect(s_tex, 0, &tex_bits, &pitch, 0);
    if (tex_bits) {
        uint32_t *pix = (uint32_t*)tex_bits;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                // Procedural pitch clay / straw turf pattern
                uint8_t r = 190 + ((x ^ y) & 7) * 3;
                uint8_t g = 170 + ((x * 5 + y * 3) & 15);
                uint8_t b = 130 + ((x + y) & 7) * 2;

                // Cricket batting/bowling crease white markings
                if (y == 12 || y == 13 || y == 50 || y == 51) {
                    r = 250; g = 250; b = 240; // White popping crease
                } else if ((x == 16 || x == 47) && y >= 12 && y <= 51) {
                    r = 240; g = 240; b = 230; // White return crease
                }

                pix[y * (pitch / 4) + x] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
        d3d8_TextureUnlockRect(s_tex, 0);
    }

    // 4. Initial Camera View Matrix
    float eye[3] = { 0.0f, 3.2f, -5.8f };
    float target[3] = { 0.0f, 0.0f, 0.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    float view_mat[16];
    mat4_lookat(view_mat, eye, target, up);
    d3d8_SetTransform(d, D3DTS_VIEW, view_mat);

    s_scene_initialized = true;
    LOGI("test_d3d8_scene_init: Successfully created VB 0x%08x, IB 0x%08x, Tex 0x%08x", s_vb, s_ib, s_tex);
    return true;
}

void test_d3d8_scene_render_frame(void) {
    if (!s_scene_initialized) {
        if (!test_d3d8_scene_init()) return;
    }

    Device *d = d3d8_android_get_device();
    const D3D8AndroidState *state = d3d8_android_get_state();
    if (!d || !state) return;

    // 1. Dynamic Projection Matrix matching Android surface aspect ratio
    float aspect = (state->height > 0) ? ((float)state->width / (float)state->height) : (16.0f / 9.0f);
    float proj_mat[16];
    mat4_perspective(proj_mat, 55.0f * (float)(M_PI / 180.0), aspect, 0.2f, 100.0f);
    d3d8_SetTransform(d, D3DTS_PROJECTION, proj_mat);

    // 2. Clear BackBuffer and Depth Buffer (Stadium sky blue)
    d3d8_Clear(d, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF14243B, 1.0f, 0);

    // 3. Begin Direct3D 8 Scene
    d3d8_BeginScene(d);

    // 4. Bind Pipeline Geometry & Shaders
    d3d8_SetStreamSource(d, 0, s_vb, sizeof(D3D8Vertex));
    d3d8_SetIndices(d, s_ib, 0);
    d3d8_SetVertexShader(d, D3DFVF_CUSTOM);

    // 5. Setup Texture & Fixed-Function Sampler States
    d3d8_SetTexture(d, 0, s_tex);
    d3d8_SetTextureStageState(d, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    d3d8_SetTextureStageState(d, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d3d8_SetTextureStageState(d, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    d3d8_SetTextureStageState(d, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    // 6. Enable Hardware Depth Testing & Render States
    d3d8_SetRenderState(d, D3DRS_ZENABLE, 1);
    d3d8_SetRenderState(d, D3DRS_ZWRITEENABLE, 1);
    d3d8_SetRenderState(d, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    d3d8_SetRenderState(d, D3DRS_CULLMODE, D3DCULL_CCW);
    d3d8_SetRenderState(d, D3DRS_LIGHTING, 0);

    // ========================================================================
    // DRAW CALL 1: Rotating 3D Cricket Pitch Marker
    // ========================================================================
    s_rot_y += 0.025f;
    if (s_rot_y > 2.0f * (float)M_PI) s_rot_y -= 2.0f * (float)M_PI;

    float rot_y[16], rot_x[16], world_box[16];
    mat4_rotate_y(rot_y, s_rot_y);
    mat4_rotate_x(rot_x, 0.25f); // Slight pitch tilt
    mat4_mul(rot_x, rot_y, world_box);
    d3d8_SetTransform(d, D3DTS_WORLD, world_box);

    // 12 triangles = 36 indices
    d3d8_DrawIndexedPrimitive(d, D3DPT_TRIANGLELIST, 0, 24, 0, 12);

    // ========================================================================
    // DRAW CALL 2: Stationary Stadium Turf Ground Plane (validates depth & multi-draw)
    // ========================================================================
    float world_ground[16];
    mat4_identity(world_ground);
    d3d8_SetTransform(d, D3DTS_WORLD, world_ground);

    // 2 triangles = 6 indices starting at index 36, referencing vertices 24-27
    d3d8_DrawIndexedPrimitive(d, D3DPT_TRIANGLELIST, 24, 4, 36, 2);

    // 7. End Scene & Present to Android WebGPU swapchain
    d3d8_EndScene(d);
    d3d8_Present(d);

    s_rendered_frames++;
}

void test_d3d8_scene_shutdown(void) {
    s_scene_initialized = false;
    s_rendered_frames = 0;
}

uint32_t test_d3d8_scene_get_frame_count(void) {
    return s_rendered_frames;
}
