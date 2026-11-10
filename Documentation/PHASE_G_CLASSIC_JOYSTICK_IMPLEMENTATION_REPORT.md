# CRICKET LEGACY — PHASE G: CLASSIC JOYSTICK IMPLEMENTATION REPORT

**Author:** Cricket Legacy Engine Integration & Input Architecture Team  
**Engineering Discipline:** Sports Simulation Input Architecture, DirectInput Device Emulation  
**Phase Baseline:** `phase-g-mobile-control-system`  
**Status:** COMPLETED & VERIFIED  

---

## 1. Overview & Architectural Purpose

Classic Joystick Mode (Mode B / Mode 1) recreates the tactile precision of a physical dual-analog gamepad for mobile players who prefer fixed thumbstick navigation and discrete tactical buttons over gesture swipes.

In the original PC release of EA SPORTS Cricket 07, the engine natively expects a DirectInput 8 gamepad conforming to the **Logitech Dual Action USB** specification (Vendor ID `0x046D`, Product ID `0xC216`). The mobile classic joystick implementation bridges Android multi-touch `MotionEvents` directly to this virtual hardware device.

```
       [ Left Thumb Touch ]                     [ Right Thumb Touch ]
                 │                                        │
                 ▼                                        ▼
    [ Virtual Joystick Base ]                   [ Virtual Action Buttons ]
(Radius: 90px * scale, Deadzone: 15%)     (LOFT, ADVANCE, DEFEND, RUN, CANCEL)
                 │                                        │
                 ▼                                        ▼
    Normalized Radial Vector:               DirectInput Button Mapping:
    r = sqrt(dx^2 + dy^2)                   - BTN_LOFT    -> DIK_LSHIFT / Pad L1
    clamp(r, deadzone, max_r)               - BTN_ADVANCE -> DIK_LCONTROL / Pad R1
    norm_x = (dx/r) * factor                - BTN_DEFEND  -> DIK_RETURN / Pad Triangle
    norm_y = (dy/r) * factor                - BTN_RUN     -> DIK_D / Pad Cross
                 │                          - BTN_CANCEL  -> DIK_S / Pad Circle
                 ▼                                        │
    DirectInput Joystick Axes:                            │
    Axis 0 (LX): norm_x * 32767                           │
    Axis 1 (LY): norm_y * 32767                           │
                 │                                        │
                 └───────────────────┬────────────────────┘
                                     ▼
                     [ input_gamepad() / input_key_dik() ]
                                     │
                                     ▼
                DirectInput 8 Polling State (`GetDeviceState`)
                                     │
                                     ▼
                    Authentic Cricket 07 Game Thread
```

---

## 2. Mathematical Processing & Radial Deadzone

A common flaw in mobile virtual joysticks is rectangular clipping or unscaled deadzones that create "jumping" velocity discontinuities. Cricket Legacy implements continuous circular deadzone scaling with radial clamping:

1. **Relative Displacement:**
   $$\Delta x = x - x_{\text{center}}, \quad \Delta y = y - y_{\text{center}}$$
   $$r = \sqrt{\Delta x^2 + \Delta y^2}$$

2. **Circular Boundary Clamping:**
   $$\text{If } r > R_{\text{max}}, \quad \Delta x \gets \frac{\Delta x}{r} R_{\text{max}}, \quad \Delta y \gets \frac{\Delta y}{r} R_{\text{max}}, \quad r \gets R_{\text{max}}$$

3. **Deadzone Re-scaling:**
   $$\text{Let } r_{\text{dead}} = R_{\text{max}} \times \delta_{\text{deadzone}} \quad (\text{default } \delta = 0.15)$$
   $$\text{If } r < r_{\text{dead}} \implies \text{Axis}_X = 0, \quad \text{Axis}_Y = 0$$
   $$\text{Else } \text{factor} = \left(\frac{r - r_{\text{dead}}}{R_{\text{max}} - r_{\text{dead}}}\right) \times S_{\text{sensitivity}}$$
   $$\text{Axis}_X = \text{clamp}\left(\frac{\Delta x}{r} \times \text{factor} \times 32767, -32768, 32767\right)$$
   $$\text{Axis}_Y = \text{clamp}\left(\frac{\Delta y}{r} \times \text{factor} \times 32767, -32768, 32767\right)$$

This guarantees that:
- Micro-finger tremors within the central $15\%$ deadzone produce zero unwanted drift.
- Crossing the deadzone boundary starts smoothly at $0$ velocity rather than jarringly jumping to $15\%$.
- Maximum stick deflection is uniformly reachable at all angles without corner clipping.

---

## 3. Right-Hand Tactical Action Cluster

In Classic Joystick mode, the right side of the screen hosts 5 primary tactile action buttons styled with semi-transparent rounded rectangular surfaces and glowing borders:

| Button Name | ID | Screen Placement | Primary Function | Authentic Engine Binding |
|:---|:---:|:---|:---|:---|
| **LOFT** | `BTN_LOFT_TOGGLE` | Upper cluster, left | Toggles aerial / ground shot mode | `DIK_LSHIFT` (`0x2A`) / Gamepad Pad 4 (LB) |
| **ADVANCE** | `BTN_ADVANCE` | Upper cluster, right | Charges down pitch toward bowler | `DIK_LCONTROL` (`0x1D`) / Gamepad Pad 5 (RB) |
| **DEFEND** | `BTN_DEFEND` | Lower cluster, left | Plays back-foot / forward defensive block | `DIK_RETURN` (`0x1C`) / Gamepad Pad 3 (Y) |
| **RUN** | `BTN_RUN` | Lower cluster, center | Calls for a single / boundary run | `DIK_D` (`0x20`) / Gamepad Pad 0 (A) |
| **CANCEL** | `BTN_CANCEL_RUN` | Lower cluster, right | Cancels run / turns batsman back | `DIK_S` (`0x1F`) / Gamepad Pad 1 (B) |

---

## 4. Multi-Touch Isolation & Zero Contention

Each pointer entering the screen is tagged with an immutable `PointerOwnerTag` (`OWNER_JOYSTICK`, `OWNER_BUTTON`, `OWNER_SWIPE_AREA`). 

- Once a thumb lands inside the joystick capture radius ($1.5 \times R_{\text{max}}$), its pointer ID is locked to the joystick.
- Moving the thumb across the screen updates only the joystick axes; it cannot inadvertently trigger buttons or swipe events even if the thumb drifts across the button boundary.
- Tapping buttons with the right thumb independently fires button state callbacks while the left thumb maintains continuous directional control.

---

## 5. Verification & Test Evidence

The classic joystick pipeline was validated on `emulator-5554` via `test_mobile_input.c`:
- **Sub-deadzone Test (5px deflection):** Stick report $(0, 0) \to$ **PASS** (Zero output, drift-free)
- **Active Range Test (60px deflection):** Stick report $(19917, 0) \to$ **PASS** (Linear smooth acceleration)
- **Centering on Release:** Stick report resets to $(0, 0) \to$ **PASS**
- **Simultaneous Stick + Button Assertions:** Stick LX active ($24200$) concurrently with `DIK_LSHIFT` ($1$) $\to$ **PASS**
