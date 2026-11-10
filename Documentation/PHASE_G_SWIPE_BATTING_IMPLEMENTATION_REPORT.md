# CRICKET LEGACY — PHASE G: SWIPE BATTING IMPLEMENTATION REPORT

**Author:** Cricket Legacy Engine Integration & Input Architecture Team  
**Engineering Discipline:** Sports Gameplay Systems, NDK Integration, DirectInput Translation  
**Phase Baseline:** `phase-g-mobile-control-system`  
**Status:** COMPLETED & VERIFIED  

---

## 1. Executive Summary

Phase G delivers a professional, tactile swipe batting system for Cricket Legacy on Android without modifying the authentic physics or gameplay logic of the recompiled EA SPORTS Cricket 07 engine. The mobile swipe system bridges the gap between capacitive touchscreen gesture dynamics and the engine's original DirectInput 8 polling architecture (`GetDeviceState`).

By intercepting touch coordinates at the NDK layer, evaluating stroke vector geometry in real time, and modulating DirectInput scan-codes and virtual gamepad axes across a precise 6-frame hold window (~100 ms), players achieve natural, responsive stroke-play spanning all 360 degrees of the cricket field.

```
       [ Touch Down ] (x0, y0, t0)
              │
              ▼
       [ Touch Move ] Tracking trajectory & calculating instantaneous velocity
              │
              ▼
       [ Touch Up ] (x1, y1, t1)
              │
       ┌──────┴──────────────────────────────┐
       ▼                                     ▼
[ Tap (< 15 px, < 150 ms) ]      [ Stroke (≥ 25 px, < 600 ms) ]
       │                                     │
Defensive Block                  Vector Classification: dx = x1 - x0, dy = y1 - y0
(DIK_RETURN pulse)               θ = atan2(-dy, dx) -> 8 Sectors
                                 Velocity v = dist / duration
                                 Loft = Toggle active OR v > 2.2 px/ms
                                             │
                                             ▼
                                DirectInput Pulse Injection:
                                Sector DIK + DIK_RETURN (+ DIK_LSHIFT if loft)
                                + Gamepad Left Stick (-32768..32767)
                                (Held across 6 frames for engine synchronization)
```

---

## 2. Sector Geometry & Compass Partitioning

The original Cricket 07 engine resolves batting strokes using directional keys (`DIK_UP`, `DIK_DOWN`, `DIK_LEFT`, `DIK_RIGHT`) in combination with the shot execution key (`DIK_RETURN`) and modifiers (`DIK_LSHIFT` for loft, `DIK_LCONTROL` for charging down the pitch).

In `mobile_input.c`, the raw screen-space displacement vector $(\Delta x, \Delta y)$ is transformed to an absolute angle $\theta \in [0, 360^\circ)$:

$$\theta = \text{atan2}(-\Delta y, \Delta x) \times \frac{180}{\pi} \pmod{360^\circ}$$

The angle is partitioned into 8 symmetrical $45^\circ$ compass sectors centered on cardinal and intercardinal bearings:

| Sector ID | Compass Angle Range | Cricket Shot Type (RHB) | DirectInput Scan-Codes | Gamepad Stick $(X, Y)$ |
|:---:|:---:|:---:|:---:|:---:|
| `SECTOR_STRAIGHT_DRIVE` | $67.5^\circ \le \theta < 112.5^\circ$ | Straight Drive / Long-off / Long-on | `DIK_UP` + `DIK_RETURN` | $(0, -32767)$ |
| `SECTOR_COVER_DRIVE` | $112.5^\circ \le \theta < 157.5^\circ$ | Cover Drive / Extra Cover | `DIK_UP` + `DIK_LEFT` + `DIK_RETURN` | $(-23170, -23170)$ |
| `SECTOR_POINT_CUT` | $157.5^\circ \le \theta < 202.5^\circ$ | Square Cut / Point | `DIK_LEFT` + `DIK_RETURN` | $(-32767, 0)$ |
| `SECTOR_THIRD_MAN` | $202.5^\circ \le \theta < 247.5^\circ$ | Late Cut / Third Man / Steer | `DIK_DOWN` + `DIK_LEFT` + `DIK_RETURN` | $(-23170, 23170)$ |
| `SECTOR_FINE_LEG` | $247.5^\circ \le \theta < 292.5^\circ$ | Fine Leg Glance / Paddle Sweep | `DIK_DOWN` + `DIK_RETURN` | $(0, 32767)$ |
| `SECTOR_SQUARE_LEG` | $292.5^\circ \le \theta < 337.5^\circ$ | Square Leg Flick / Pull | `DIK_RIGHT` + `DIK_RETURN` | $(32767, 0)$ |
| `SECTOR_MID_WICKET` | $337.5^\circ \le \theta \lor \theta < 22.5^\circ$ | Cow Corner / Mid-Wicket Slog | `DIK_UP` + `DIK_RIGHT` + `DIK_RETURN` | $(23170, -23170)$ |
| `SECTOR_MID_ON` | $22.5^\circ \le \theta < 67.5^\circ$ | On Drive / Mid-on | `DIK_UP` + `DIK_RIGHT` + `DIK_RETURN` | $(23170, -23170)$ |

---

## 3. Stroke Dynamics & Velocity Modulation

A key finding from competitor research (Real Cricket and WCC3) is that players desire both deliberate control over lofting and expressive physical gestures. Cricket Legacy implements a dual-mode lofting pipeline:

1. **Explicit Loft Toggle (`BTN_LOFT_TOGGLE`):**
   - Tapping the `LOFT` button in the HUD illuminates the button in glowing amber.
   - When engaged, any swipe gesture automatically injects `DIK_LSHIFT` (`0x2A`) alongside the directional stroke.
2. **Dynamic Velocity Trigger:**
   - Stroke velocity $v = \frac{\text{distance (px)}}{\Delta t \text{ (ms)}}$.
   - If a swipe exceeds $2.2\text{ px/ms}$ ($\sim 2200\text{ px/s}$), the stroke recognizer treats the aggressive physical flick as a deliberate intention to loft, automatically appending `DIK_LSHIFT` and vibrating the device via a short haptic burst.
3. **Defensive Strokes (Tap Block):**
   - If touch displacement is less than $15\text{ px}$ and duration is less than $150\text{ ms}$, the gesture is classified as a defensive block, pulsing `DIK_RETURN` without direction keys to trigger the batsman's forward defensive posture.

---

## 4. Engine Synchronization Window (The 6-Frame Hold Buffer)

In PC DirectInput games, a keyboard key is held for several tens or hundreds of milliseconds. In an Android touch event stream, a stroke `ACTION_UP` occurs instantaneously at a single timestamp. If scan-codes were toggled high and immediately low in the same callback, the original game loop (running at 60 FPS in its separate thread) would miss the input between polling intervals.

To guarantee deterministic capture:
- When a stroke triggers, the active keys are written to `s_active_stroke_keys[]`.
- `s_stroke_key_hold_frames` is initialized to `6`.
- On every engine frame tick (`mobile_input_frame_tick()`), the counter decrements.
- On reaching zero, all active stroke keys are released.

This guarantees a stable $100\text{ ms}$ input assertion window, completely eliminating "dropped shot" anomalies during bowling deliveries.

---

## 5. Verification & Test Evidence

The swipe batting pipeline was rigorously verified using automated integration testing directly on the Android runtime target (`emulator-5554` via `test_mobile_input`):

- **Straight Drive Test:** Vector $(0, -100) \to 90.0^\circ \to$ Sector 0 (`SECTOR_STRAIGHT_DRIVE`) $\to$ `DIK_UP` (`0xC8`) + `DIK_RETURN` (`0x1C`) asserted $\to$ **PASS**
- **Cover Drive Test:** Vector $(-70, -70) \to 135.0^\circ \to$ Sector 1 (`SECTOR_COVER_DRIVE`) $\to$ `DIK_UP` + `DIK_LEFT` (`0xCB`) + `DIK_RETURN` asserted $\to$ **PASS**
- **Square Cut Test:** Vector $(-100, 0) \to 180.0^\circ \to$ Sector 2 (`SECTOR_POINT_CUT`) $\to$ `DIK_LEFT` + `DIK_RETURN` asserted $\to$ **PASS**
- **Third Man Test:** Vector $(-70, 70) \to 225.0^\circ \to$ Sector 3 (`SECTOR_THIRD_MAN`) $\to$ `DIK_DOWN` + `DIK_LEFT` + `DIK_RETURN` asserted $\to$ **PASS**
- **Fine Leg Test:** Vector $(0, 100) \to 270.0^\circ \to$ Sector 4 (`SECTOR_FINE_LEG`) $\to$ `DIK_DOWN` + `DIK_RETURN` asserted $\to$ **PASS**
- **Square Leg Test:** Vector $(70, 70) \to 315.0^\circ \to$ Sector 5 (`SECTOR_SQUARE_LEG`) $\to$ `DIK_RIGHT` + `DIK_RETURN` asserted $\to$ **PASS**
- **Mid-Wicket Pull Test:** Vector $(100, 0) \to 0.0^\circ \to$ Sector 6 (`SECTOR_MID_WICKET`) $\to$ `DIK_UP` + `DIK_RIGHT` + `DIK_RETURN` asserted $\to$ **PASS**
- **Mid-On Drive Test:** Vector $(70, -70) \to 45.0^\circ \to$ Sector 7 (`SECTOR_MID_ON`) $\to$ `DIK_UP` + `DIK_RIGHT` + `DIK_RETURN` asserted $\to$ **PASS**
- **Hold Window Verification:** All scan-codes held across 6 frames and confirmed zeroed at frame 7 $\to$ **PASS**
