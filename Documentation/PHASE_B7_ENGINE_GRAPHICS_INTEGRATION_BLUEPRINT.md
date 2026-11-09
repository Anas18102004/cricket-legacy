# Phase B.7: Engine Graphics Integration Blueprint

**Project:** Cricket Legacy (EA SPORTS Cricket 07 Mobile Port)  
**Milestone:** Phase B.7 Native Validation  
**Date:** November 2026  
**Status:** **ARCHITECTURAL BLUEPRINT COMPLETE**  
**Source Code Baseline:** `Dhaiwat10/cricket07-browser` (`host/d3d8.c`, `host/render_wgpu.c`, `host/render_cmd.h`)  
**Target Mobile Graphics Backend:** `wgpu-native` on Android Vulkan / iOS Metal  

---

## 1. Executive Summary

This blueprint details the architectural bridge connecting the 31,548 recompiled C functions of EA SPORTS Cricket 07 to our validated Android WebGPU rendering engine.

In Phase B.7, we proved that `wgpu-native` operates with native hardware acceleration on Android 14 via Vulkan, presenting frames directly to `ANativeWindow`. Concurrently, our audit of `Dhaiwat10/cricket07-browser` revealed that the project already includes a complete Direct3D 8 to WebGPU translation layer (`host/d3d8.c` and `host/render_wgpu.c`). 

Instead of writing a custom graphics engine or rewriting the game's rendering pipeline, Phase C can integrate `host/d3d8.c` and `host/render_wgpu.c` directly into the Android native harness with **minimal modification**.

---

## 2. Upstream Architecture Analysis

In the PC version of EA SPORTS Cricket 07, graphics operations call the DirectX 8 runtime (`d3d8.dll`). In `cricket07-browser`, these calls are intercepted and redirected via a two-layer decoupled design:

```
+-----------------------------------------------------------+
|          31,548 Recompiled Game Functions (out_all/*.c)   |
+-----------------------------+-----------------------------+
                              | Calls D3D8 API
                              v
+-----------------------------------------------------------+
|             D3D8 Emulation Shim (host/d3d8.c)             |
|   Records draw calls, state changes, texture uploads       |
+-----------------------------+-----------------------------+
                              | Emits command stream (render_cmd.h)
                              v
+-----------------------------------------------------------+
|            WebGPU Render Executor (host/render_wgpu.c)    |
|   Translates commands -> WebGPU API -> wgpu-native        |
+-----------------------------+-----------------------------+
                              | Hardware GPU API
                              v
+-----------------------------------------------------------+
|       Android ANativeWindow Surface (Vulkan Backend)      |
+-----------------------------------------------------------+
```

### 2.1 Direct3D 8 Command Stream (`host/render_cmd.h`)
The command recording layer serializes D3D8 operations into lightweight command packets:
- `CMD_TARGET`: Sets render target texture or backbuffer.
- `CMD_CLEAR`: Clears color, depth, and stencil buffers.
- `CMD_DRAW`: Indexed/non-indexed primitive drawing (`D3DPT_TRIANGLELIST`, `TRIANGLESTRIP`, etc.).
- `CMD_VS_PROGRAM`: Uploads vertex shader declarations and constants.
- `CMD_UPLOAD_TEX`: Uploads mipmapped textures.
- `CMD_DESTROY_TEX`: Destroys texture views and textures.
- `CMD_PRESENT`: Triggers backbuffer swapchain presentation.

### 2.2 WGSL "Uber Shader" Pipeline (`host/render_wgpu.c`)
`render_wgpu.c` implements a comprehensive WGSL "uber shader" that replicates the Direct3D 8 fixed-function vertex and fragment pipeline:
- Direct3D viewport transformation to NDC space.
- World-View-Projection (`MVP`) 4x4 matrix uniforms.
- Alpha testing (`CMP_GREATER`, `CMP_LESSEQUAL`, etc.) and alpha blending.
- Texture blending stages (`COLOROP`, `COLORARG1`, `COLORARG2`).
- Material ambient and diffuse lighting.
- Programmable pixel shader 1.1 emulation via `host/pshader.c`.

---

## 3. Concrete Android Native Porting Plan

To transition `host/d3d8.c` and `host/render_wgpu.c` from desktop macOS/Linux to Android native:

### Step 1: Surface Binding Replacement
In desktop builds, `render_wgpu.c` instantiates surfaces using macOS Metal layers or SDL2 window handles. We replace this with our validated Android surface creator:

```c
// android-native-prototype/src/cricket_wgpu_surface.c
WGPUSurface create_android_wgpu_surface(WGPUInstance instance, ANativeWindow* window) {
    WGPUSurfaceDescriptorFromAndroidNativeWindow android_desc = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow
        },
        .window = window
    };
    WGPUSurfaceDescriptor desc = {
        .nextInChain = (const WGPUChainedStruct*)&android_desc,
        .label = "CricketLegacy_Android_Surface"
    };
    return wgpuInstanceCreateSurface(instance, &desc);
}
```

### Step 2: Dynamic Surface Capability Negotiation
As demonstrated in Phase B.7, Android Vulkan requires dynamic alpha mode selection:
- Inherit / Auto (`WGPUCompositeAlphaMode_Inherit`) rather than hardcoded Opaque.
- `BGRA8Unorm` surface format.
- Fifo present mode for smooth 60 FPS v-sync pacing.

### Step 3: Threading & Execution Model
In the browser, WebAssembly execution is asynchronous due to browser thread yielding. On Android native:
- **Direct Synchronous Mode:** The game thread can record and immediately execute commands on the WebGPU device without thread hopping or message passing.
- **Render Loop:** Synchronized with Android's `Choreographer` via JNI or native `ALooper` to achieve locked 60 FPS frame pacing.

---

## 4. Texture and Asset Streaming Pipeline

Cricket 07 stores game assets (stadiums, player models, pitches, crowd textures) in BIG files (`.big`, `.fsh`).
1. **Texture Conversion:** `host/texconv.c` decodes EA DXT/palette FSH textures to raw RGBA8.
2. **GPU Texture Upload:** RGBA8 buffers are written into WebGPU 2D textures using `wgpuQueueWriteTexture`.
3. **Sampler Configuration:** Direct3D 8 address modes (`CLAMP`, `WRAP`) and filters (`LINEAR`, `POINT`) map directly to `WGPUSamplerDescriptor`.

---

## 5. Phase C Roadmap: Engine Integration Milestones

```
+-------------------------------------------------------------------------------+
| PHASE C.1: UPSTREAM GRAPHICS LAYER PORT                                      |
| Integrate host/d3d8.c, host/render_wgpu.c, and host/pshader.c into Android     |
| native harness. Wire to existing ANativeWindow wgpu-native Vulkan context.   |
+---------------------------------------+---------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
| PHASE C.2: ASSET SYSTEM & VFS (BIG / FSH)                                    |
| Adapt host/misc.c and host/texconv.c to stream original PC assets from APK    |
| assets/ directory into the 4GB Virtual Memory Arena.                          |
+---------------------------------------+---------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
| PHASE C.3: 31,548 RECOMPILED ENGINE FUNCTIONS                                 |
| Compile out_all/*.c into libcricket_native_harness.so using NDK Clang 18.      |
| Connect g_mem to Strategy 2 Virtual Memory Arena.                             |
+---------------------------------------+---------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
| PHASE C.4: IN-GAME MATCH PLAY & MOBILE TOUCH CONTROLS                         |
| Replace DirectInput keyboard/mouse hooks with mobile touchscreen virtual      |
| analog bat/bowl controls; run full match on Android 14.                       |
+-------------------------------------------------------------------------------+
```

---

## 6. Conclusion

The path from Phase B.7 to full game execution is clear, direct, and minimizes speculative engineering:
1. The memory model is validated and conforms to the recompiler's architecture.
2. `wgpu-native` operates on Android with hardware Vulkan acceleration.
3. The upstream game already possesses a WebGPU Direct3D 8 renderer.

We are ready to proceed with Phase C.
