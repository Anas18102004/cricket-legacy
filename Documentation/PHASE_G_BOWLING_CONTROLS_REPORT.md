# CRICKET LEGACY — PHASE G: BOWLING CONTROLS IMPLEMENTATION REPORT

**Author:** Cricket Legacy Engine Integration & Input Architecture Team  
**Engineering Discipline:** Sports Bowling Mechanics, Pitch Marker Projection, Timing Meter Integration  
**Phase Baseline:** `phase-g-mobile-control-system`  
**Status:** COMPLETED & VERIFIED  

---

## 1. Executive Summary

Bowling in cricket demands millimeter-level precision in line, length, seam angle, and release timing. In the authentic EA SPORTS Cricket 07 engine, bowling is governed by a 3-step sequence:
1. **Delivery Type Selection:** Standard (`DIK_S`), Swing (`DIK_A`), Cutter (`DIK_D`), or Bouncer/Slower (`DIK_W`).
2. **Pitch Marker Placement:** Navigating the pitch landing circle using direction keys or analog stick.
3. **Power & Accuracy Meter Timing:** Starting the run-up and hitting the sweet release bar on the vertical bowling meter using `DIK_RETURN` or Button A.

Phase G implements two distinct bowling control modes:
- **Mode A: Gesture Pitch Aiming (Touch-to-Place + Swipe Release)**
- **Mode B: Classic Reticle Aiming (Stick + Action Buttons)**

```
       [ Step 1: Delivery Selection ]
       Tactile HUD Buttons: STANDARD (S) | SWING (A) | CUTTER (D) | BOUNCER (W)
                     │
                     ▼
       [ Step 2: Pitch Marker Aiming ]
       Touch Pitch directly OR Deflect Left Stick to move landing spot
                     │
                     ▼
       [ Step 3: Run-up & Meter Timing ]
       Tap Bowler / Meter -> Bowler approaches wicket
       Tap at Top of Meter -> Optimal speed & swing (Haptic pulse fires!)
```

---

## 2. Touch-to-Pitch Marker Projection

When using Gesture Aim Mode (Mode A), the player can directly touch the 3D pitch on screen to position the ball landing marker:

1. **Screen-to-Pitch Raycast Projection:**
   The camera in Cricket 07 uses an authentic perspective projection matrix $P \times V$. Screen touch coordinates $(x_{\text{touch}}, y_{\text{touch}})$ are converted into normalized device coordinates (NDC) and intersected with the ground plane ($Z = 0$):

   $$x_{\text{pitch}} = \text{clamp}\left(x_{\text{world}}, -1.5\text{ m}, 1.5\text{ m}\right) \quad (\text{Off stump to Leg stump})$$
   $$y_{\text{pitch}} = \text{clamp}\left(y_{\text{world}}, 1.2\text{ m}, 18.0\text{ m}\right) \quad (\text{Yorker to Short Bouncer})$$

2. **DirectInput Aim Emulation:**
   The difference between the current engine cursor position and the target touch coordinate is converted into directional key pulses (`DIK_UP`, `DIK_DOWN`, `DIK_LEFT`, `DIK_RIGHT`) or analog stick axis deflections until the cursor reaches the touched location.

---

## 3. Delivery Type Matrix & HUD Buttons

When in `GAME_PHASE_BOWLING`, the HUD displays 4 dedicated bowling delivery buttons:

| Delivery Button | DirectInput Key | Gamepad Button | Fast Bowler Effect | Spin Bowler Effect |
|:---|:---:|:---:|:---|:---|
| **STANDARD** | `DIK_S` (`0x1F`) | Button 1 (A) | Normal seam delivery, high pace | Standard off-break / leg-break |
| **SWING** | `DIK_A` (`0x1E`) | Button 2 (B) | Conventional outswing into the batsman | Top spinner / slider |
| **CUT** | `DIK_D` (`0x20`) | Button 3 (X) | Off-cutter or leg-cutter off pitch seam | Arm ball / googly |
| **BOUNCER** | `DIK_W` (`0x11`) | Button 4 (Y) | Steep bounce short delivery or knuckle slower | Flipper / doosra |

---

## 4. Run-up & Meter Release Timing

Once line and length are locked:
1. Tapping the pitch or delivery button initiates the bowler's run-up (`DIK_RETURN` asserted).
2. The authentic Cricket 07 vertical meter fills upward.
3. As the meter needle enters the sweet green zone (within $5\%$ of peak velocity), the player taps the release zone.
4. `mobile_input` injects `DIK_RETURN` (`0x1C`) to lock in maximum pace and sharp swing.
5. `nativeCricketPollHaptic()` triggers a sharp $25\text{ ms}$ haptic pulse, giving physical confirmation of a perfect delivery.

---

## 5. Verification & Test Evidence

- **Delivery Type Triggering:** Tested all 4 delivery buttons (`DIK_S`, `DIK_A`, `DIK_D`, `DIK_W`). Verified correct scan-code injection and engine animation response.
- **Run-up Initiation & Delivery Stride:** Tested timing release at frame 3450 (`DIK_S`) and frame 3540 (`DIK_RETURN`). Captured on-device screenshot `f3_bowling_delivery.png` showing bowler completing delivery stride and releasing ball.
- **Haptic Confirmation:** Verified that perfect release timing prompts the Android `Vibrator` service.
