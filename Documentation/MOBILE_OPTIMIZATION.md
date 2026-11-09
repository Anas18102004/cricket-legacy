# CRICKET LEGACY — Mobile Optimization & Performance Specification

---

## 1. Frame Rate & Pacing Targets

| Tier | Target Devices (Reference) | Target Frame Rate | P90 Max Frame Time | P99 Max Frame Time |
| :--- | :--- | :--- | :--- | :--- |
| **High / Flagship** | Snapdragon 8 Gen 2/3, Apple A16/A17/M1+ | **60 FPS** | $16.0\text{ ms}$ | $18.5\text{ ms}$ |
| **Mid-Range** | Snapdragon 778G / Dimensity 8100 | **60 FPS** | $16.6\text{ ms}$ | $20.0\text{ ms}$ |
| **Budget / Entry** | Helio G99 / Snapdragon 680 | **30 FPS (Cap)** | $31.5\text{ ms}$ | $35.0\text{ ms}$ |

---

## 2. Rendering & Draw Call Budget

- **Maximum Batched Draw Calls:** $\le 120$ per frame in active match camera view.
- **Frustum Geometry Budget:** $\le 350,000$ vertices rendered per frame.
- **Texture Compression:**
  - Standard: ASTC 6x6 for characters and stadium hero assets.
  - Normal maps: ASTC 5x5 or ASTC 4x4.
  - UI / 2D Assets: ASTC 4x4 with alpha channel.
- **Maximum Texture VRAM:** $\le 250\text{ MB}$ total runtime texture footprint.
- **URP Settings:**
  - Rendering Path: URP Forward+ (permits clustered lighting for floodlights during Day-Night matches without multiple additive passes).
  - Shadows: 1 Real-time directional light casting soft shadows on pitch ($2048\times 2048$ cascade on High, $1024\times 1024$ on Mid, Off/Hard on Low).
  - Dynamic Resolution Scaling: URP Dynamic Resolution enabled (scales down render viewport between $80\%$ and $100\%$ if frame drops are detected).

---

## 3. Crowd & Stadium Optimization

- **Outer Stadium Bowl:** Rendered using GPU Instancing with billboard quad cards using custom vertex-shader undulating crowds (`WavingCrowd.shader`).
- **Inner Ring Spectators:** Low-poly models ($< 600$ tris per spectator) using single shared texture atlas with GPU Mesh Instancing.
- **Grass Pitch Shader:** Custom stylized grass shader using subsurface scattering approximation without shell-and-fin geometry passes.

---

## 4. Memory & Garbage Collection (GC) Architecture

- **Zero Allocation in Game Loop:**
  - Strictly no `new` allocations inside `Update()`, `FixedUpdate()`, or `LateUpdate()`.
  - Ball physics calculations executed using stack-allocated C# `struct` math primitives.
  - Event subscriptions utilize static delegates or non-allocating cached Action references.
- **Object Pooling:**
  - Pre-warmed pools for: Audio sources, ball trailing particles, turf kick-up decals, bat-ball impact sparks, and UI scorecard overlay rows.
- **Memory Footprint:** Resident memory cap strictly $< 850\text{ MB}$ RAM on Android to guarantee stability on 3GB/4GB Android devices.

---

## 5. Thermal Throttling & Battery Management

- Target device surface temperature $< 41^\circ\text{C}$ over a continuous 45-minute play session.
- Automatic battery saver trigger: Drops target frame rate to 30 FPS when device reports thermal status `SEVERE` via Android API or iOS power state notification.
