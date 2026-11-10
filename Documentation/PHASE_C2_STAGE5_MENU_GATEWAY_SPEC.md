# CRICKET LEGACY — Phase C.2 Stage 5: Host Menu Presentation & Render Gateway Specification
**Document ID:** `CL-C2-STAGE5-SPEC-v1.0`  
**Classification:** System Architecture, D3D8 Fixed-Function 2D Pipeline & Input Gateway  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** APPROVED & EMPIRICALLY VERIFIED  

---

## 1. Executive Overview & Milestone Definition

In the original EA SPORTS Cricket 07 engine, the game's graphical presentation is bifurcated into two major operating modes:
1. **The 3D In-Match Simulation:** A perspective-projected 3D scene rendering player models, stadium geometry, turf ground planes, and dynamic cameras using vertex buffers, index buffers, and lighting.
2. **The 2D Front-End Menu & HUD System:** A fixed 4:3 aspect ratio coordinate space ($640 \times 480$) rendered using pre-transformed vertices (`D3DFVF_XYZRHW | D3DFVF_DIFFUSE`), procedural/texture-mapped cards, button hover states, and an interactive pointer tracked at the global variable `PROFILE_CURSOR` (`0x00A01970`).

### Critical Architectural Boundary & Honest Framing
Under **Option B**, Stage 5 is strictly scoped and implemented as the **Host Menu Presentation & Render Gateway**.
- **What Stage 5 Is:** A fully functional, production-ready host-level demonstration menu that validates the complete 2D orthographic/transformed viewport scaling, Direct3D 8 pipeline integration, COM dispatch mechanism, touchscreen-to-cursor steering, and live linkage to the authentic `PROFILE_CURSOR` global variable memory.
- **What Stage 5 Is NOT:** It is **NOT** the proprietary, copyrighted EA SPORTS Cricket 07 game menu. The original EA assets, textures, fonts, and game executable logic are proprietary EA intellectual property and are not bundled in this open repository. Stage 5 demonstrates the complete platform gateway ready to receive translated game code.

With the successful empirical verification of Stage 5 on Android 14, **all 5 planned host infrastructure preparation stages (Stages 1 through 5) are 100% complete**.

---

## 2. Coordinate Space Architecture & Widescreen Scaling

```
                                Native Android Screen (e.g. 2400x1080 / 1920x1080)
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Pillarbox Left   │                    4:3 Menu Stage Area (640x480 Virtual)     │ Pillarbox Right│
│  (0x040A14)      │                                                              │  (0x040A14)    │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │     HEADER BANNER: "CRICKET LEGACY - STAGE 5 DEMO"     │  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
│                  │                                                              │                │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │ [0] PLAY EXHIBITION MATCH   (Hover / Pressed State)     │  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │ [1] TOURNAMENT MODE                                     │  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │ [2] SETTINGS & CONTROLS                                 │  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │ [3] SYSTEM DIAGNOSTICS                                  │  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
│                  │                                       ◆ [PROFILE_CURSOR]    │                │
│                  │                                         (0x00A01970)         │                │
│                  │  ┌────────────────────────────────────────────────────────┐  │                │
│                  │  │ FOOTER STATUS: "HOST INFRASTRUCTURE VERIFIED (STAGES 1-5│  │                │
│                  │  └────────────────────────────────────────────────────────┘  │                │
└──────────────────┴──────────────────────────────────────────────────────────────┴────────────────┘
```

### 2.1 Viewport Transformation Equations
For any native Android resolution with width $W$ and height $H$:
1. **Aspect Scale Factor ($S$):**
   $$S = \min\left(\frac{W}{640.0}, \frac{H}{480.0}\right)$$
2. **Horizontal and Vertical Centering Margins ($X_0, Y_0$):**
   $$X_0 = \frac{W - 640.0 \times S}{2.0}, \quad Y_0 = \frac{H - 480.0 \times S}{2.0}$$
3. **Screen to Virtual Coordinate Transformation:**
   $$V_x = \text{clamp}\left(\frac{S_x - X_0}{S}, 0.0, 640.0\right), \quad V_y = \text{clamp}\left(\frac{S_y - Y_0}{S}, 0.0, 480.0\right)$$
4. **Virtual to Screen Coordinate Transformation:**
   $$S_x = X_0 + V_x \times S, \quad S_y = Y_0 + V_y \times S$$

---

## 3. Direct3D 8 Pre-transformed 2D Pipeline

### 3.1 Flexible Vertex Format (FVF) Specification
In Direct3D 8, 2D screen quads bypass the 3D vertex transform pipeline (World, View, Projection matrices) by specifying `D3DFVF_XYZRHW`:
- `D3DFVF_XYZRHW = 0x004` (Pre-transformed $(X, Y, Z, RHW)$)
- `D3DFVF_DIFFUSE = 0x040` (32-bit ARGB packed vertex color)
- `D3DFVF_TEX1 = 0x100` (Single 2D texture coordinate pair $(U, V)$)
- **Combined FVF:** `D3DFVF_MENU_2D = 0x144`

```c
typedef struct {
    float x, y, z, rhw; // 16 bytes: screen x, y, depth z (0.0f), rhw (1.0f)
    uint32_t diffuse;   // 4 bytes: 0xAARRGGBB packed color
    float u, v;         // 8 bytes: normalized texture coordinates in [0.0, 1.0]
} MenuVertex2D; // Exactly 28 bytes per vertex
```

### 3.2 WebGPU Fixed-Function Shader Mapping
In `render_wgpu.c`, when `u.flags.x == 1u` (`L.pos_type == 0x4` for `XYZRHW`), the WGSL vertex shader normalizes screen coordinates directly to Normalized Device Coordinates (NDC) using viewport dimensions:
```wgsl
let w = select(1.0 / v.pos.w, 1.0, v.pos.w == 0.0);
let nx = (v.pos.x + 0.5 - u.vp.x) / u.vp.z * 2.0 - 1.0;
let ny = 1.0 - (v.pos.y + 0.5 - u.vp.y) / u.vp.w * 2.0;
o.pos = vec4<f32>(nx * w, ny * w, v.pos.z * w, w);
o.diffuse = d3dcolor(v.diffuse);
o.tc0 = v.tc0;
```

---

## 4. `PROFILE_CURSOR` Linkage & Touch Steering

### 4.1 Memory Architecture
The original Cricket 07 engine tracks the interactive menu cursor position as two IEEE 754 32-bit floats at guest address `0x00A01970`:
- **`0x00A01970`:** Virtual Cursor $X$ coordinate (float, $[0.0, 640.0]$)
- **`0x00A01974`:** Virtual Cursor $Y$ coordinate (float, $[0.0, 480.0]$)

### 4.2 Touch Steering Protocol
When Android touch events occur:
1. `MainActivity.nativeCricketOnTouch(action, x, y)` receives raw physical screen coordinates.
2. Coordinates are transformed through `game_menu_screen_to_virtual()`.
3. Coordinates are clamped strictly within the virtual $640 \times 480$ bounds.
4. The resulting IEEE 754 float bit patterns are written directly to `PROFILE_CURSOR` via `W32(PROFILE_CURSOR + 0, cx_bits)` and `W32(PROFILE_CURSOR + 4, cy_bits)`.
5. On every frame render tick, `game_menu_render_frame()` reads the live coordinates from `PROFILE_CURSOR` and positions the golden cursor pointer diamond quad dynamically on screen.

---

## 5. Procedural Texture Atlas Architecture

A $512 \times 512$ procedural texture atlas (`FMT_A8R8G8B8`) is synthesized on device startup:
- **Region 1 `[10, 10]` to `[500, 75]`:** Header banner with deep navy gradient, golden border, and embedded 16-point title text.
- **Regions 2-5 `[10, 90]` to `[400, 320]`:** 4 Menu Button cards:
  1. `PLAY EXHIBITION` (Cyan accent)
  2. `TOURNAMENT MODE` (Amber accent)
  3. `SETTINGS & CONTROLS` (Emerald accent)
  4. `SYSTEM DIAGNOSTICS` (Rose accent)
- **Region 6 `[10, 335]` to `[500, 410]`:** System status panel displaying memory arena state, D3D8 COM dispatch telemetry, and cursor coordinates.
- **Region 7 `[420, 90]` to `[460, 130]`:** Interactive cursor diamond pointer icon with high-visibility gold-and-white core.
- **Region 8 `(508, 508)` to `(512, 512)`:** Solid white pixel block allowing untextured solid geometry fills to share the same draw pipeline.

---

## 6. Button Hit-Testing & Action Dispatch

```c
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
        }
        b->pressed = false;
    }
}
```

---

## 7. Direct3D 8 Rendering Gateway Flow

Each frame rendered through `game_menu_render_frame()` executes the following sequence:
1. `update_viewport_metrics()` recalculates scaling for current screen resolution.
2. Live cursor coordinates are retrieved from `PROFILE_CURSOR` (`0x00A01970`).
3. Dynamic vertex buffer is locked (`d3d8_BufferLock`).
4. Geometry quads (background, stage panel, header, 4 buttons, hover borders, status footer, cursor diamond) are emitted with screen-space coordinates.
5. Vertex buffer is unlocked (`d3d8_BufferUnlock`).
6. D3D8 scene is cleared (`d3d8_Clear`).
7. `d3d8_BeginScene()` opens the render pass.
8. Pipeline states (`SetStreamSource`, `SetIndices`, `SetVertexShader(0x144)`, `SetTexture`) are bound.
9. Render states are configured (`ZENABLE = 0`, `CULLMODE = NONE`, `LIGHTING = 0`, `COLOROP = MODULATE`).
10. `d3d8_DrawIndexedPrimitive()` executes the single batched draw call.
11. `d3d8_EndScene()` closes the render pass.
12. `d3d8_Present()` presents the backbuffer to the Android WebGPU swapchain.
