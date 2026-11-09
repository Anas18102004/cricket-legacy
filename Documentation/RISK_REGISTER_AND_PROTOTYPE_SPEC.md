# CRICKET LEGACY — Risk Register & Prototype 0.1 Specification

---

## 1. Prototype 0.1: "The Cricket Ground" Specification

### 1.1 Objective & Scope
The purpose of Prototype 0.1 is to establish the minimum viable interactive cricket slice, validating physics-driven ball flight and touchscreen batting/bowling response before committing to full art and match mode production.

### 1.2 Prototype 0.1 Feature Checklist

```
[ ] 1. Stadium Pitch Environment
    - Standard 22-yard 3D pitch mesh with popping crease and bowling crease lines.
    - 3D Stumps (Off, Middle, Leg) and bails with rigid body physics on collision.
    - Simplified boundary rope circle (radius 65m).

[ ] 2. Core Physics Subsystem
    - Ball launched at selectable speeds (100 - 150 km/h).
    - 120Hz RK4 ballistics substepping with aerodynamic drag.
    - Pitch bounce with restitution and friction.
    - Swept continuous bat collision detection.

[ ] 3. Batting Control & Feedback
    - Swipe gesture recognition (Straight, Off, Leg, Lofted).
    - Timing window indicator (Early, Perfect, Late, Miss).
    - Edge deflection vs sweet-spot solid hit differentiation.

[ ] 4. Bowling Control
    - Line/Length targeting reticle on pitch.
    - Release timing bar determining delivery speed and swing amount.

[ ] 5. Score & State Validation
    - Automatic detection of Dot Ball, 1-6 Runs, Boundary (4 / 6), Bowled, and Caught.
    - Real-time HUD showing current delivery telemetry (Speed, Bounce Height, Contact Exit Velocity).
```

### 1.3 Prototype Acceptance Gate
- A user can face 12 consecutive deliveries on a physical Android touchscreen and hit at least 4 distinct shot types with discernible differences in ball trajectory.
- The ball never tunnels through the bat or stumps even at $155\text{ km/h}$.
- Frame pacing maintains $\ge 60\text{ FPS}$ continuously throughout the delivery-to-boundary sequence.

---

## 2. Risk Register & Mitigation Strategy

| Risk ID | Risk Description | Severity | Probability | Mitigation Strategy |
| :--- | :--- | :--- | :--- | :--- |
| **RSK-01** | **Touchscreen Timing Latency:** Screen touch input delay ($30\text{ ms} - 60\text{ ms}$) on Android causing mistimed shots. | **Critical** | High | Implement Predictive Touch Input & Input Timestamp Compensation using Unity's `EnhancedTouch` API. Bat swing animation interpolates contact point using hardware input event timestamps. |
| **RSK-02** | **High-Velocity Ball Tunneling:** Ball passing through bat collider at speeds over $145\text{ km/h}$. | **Critical** | Medium | Use 120Hz internal physics substepping combined with swept ray/capsule continuous collision detection rather than discrete frame colliders. |
| **RSK-03** | **Animation-Physics Disconnect:** Bat model visually missing the ball even when collision triggers, or vice-versa. | **High** | High | Implement Two-Bone IK and Procedural Bat Angle adjustments in LateUpdate, clamping the bat blade directly toward the dynamic ball intercept plane. |
| **RSK-04** | **Budget GPU Fill-Rate Bottlenecks:** Stadium crowd and dynamic lighting causing thermal throttling on Mali/Adreno budget GPUs. | **High** | Medium | Utilize GPU Instancing with single-quad billboard crowds in upper stands; employ URP Forward+ with a single real-time shadow cascade; disable real-time screen-space reflections. |
| **RSK-05** | **Intellectual Property & Copyright Issues:** Accidental infringement of EA SPORTS Cricket 07 branding, commentary, or assets. | **Critical** | Low | Strictly enforce 100% original C# codebase, custom 3D models, procedural audio/original voices, and unique fictional team/player rosters. No EA assets or names permitted in repository. |
| **RSK-06** | **Multi-Touch Gesture Ambiguity:** Palm rejection or thumb resting on screen causing false shot swipe triggers. | **Medium** | High | Implement spatial touch filtering dividing screen into exclusive left (analog footwork) and right (stroke gesture) bounding zones, with minimum velocity and distance thresholds for swipe registration. |
