# Phase G.5 / G.6 — Side-by-Side Visual Audit

## Reference Review and Design Principles

The mobile control ergonomics are inspired by modern titles like Real Cricket and WCC3:
1. Left side thumb-zone: Circular directional pad / joystick clamped to viewport.
2. Right side action cluster: `LOFT`, `ADVANCE`, `DEFEND` elevated above the lower broadcast scoreboard (>32 dp clearance).
3. Central corridor: Complete pitch view from bowler to batsman remains 100% unobstructed.
4. Fail-closed architecture: All mobile HUD overlays hide immediately on menus, cutscenes, and 2D panels.

## Actual Runtime In-Match Evidence (Final Verified Build)

All captures below were recorded directly from `emulator-5554` running the recompiled EA SPORTS Cricket 07 3D engine in live open match play at Lord's (England vs New Zealand / South Africa).

| Context / View | Artifact Path | Resolution | Observations & Verification |
|---|---|---|---|
| **Classic Joystick Batting** | `artifacts/phase-g5-validation/g5_classic_joystick_match.png` | 2264×1017 | Viewport-clamped joystick ($R \le 84\text{ dp}$, $r_{\text{deadzone}}=0.15R$) on lower-left thumb zone. Right action cluster (`LOFT`, `ADVANCE`, `DEFEND`) positioned with clean clearance above broadcast ticker. Pitch corridor completely unobstructed. |
| **In-Flight Active Swipe** | `artifacts/phase-g5-validation/g5_swipe_flight.png` | 2264×1017 | Live in-flight swipe trail captured mid-stroke with dynamic floating sector badge (`STRAIGHT DRIVE`). Directional line and terminus circle render with high-contrast translucent styling. Disappears instantly upon finger release. |
| **Running Controls** | `artifacts/phase-g5-validation/g5_running_match.png` | 2264×1017 | Context-specific `RUN` and `CANCEL` controls with massive touch targets (>48 dp). No joystick shown. 3D batsman and pitch scene fully visible. |
| **Bowling Controls** | `artifacts/phase-g5-validation/g5_bowling_match.png` | 2264×1017 | Full cricket stadium view with bowling action cluster (`STANDARD`, `SWING`, `CUT`, `BOUNCER`). Scoreboard completely unobstructed. |
| **Pause / End of Over Menu** | `artifacts/phase-g5-validation/screen_current.png` | 2264×1017 | Fail-closed verification: Upon reaching the End of Over menu panel, the mobile HUD automatically hides completely. Touch on "Resume" interacts directly with the original game menu and smoothly restores 3D play. |

## Visual Audit Summary

- **Scoreboard Clearance:** 100% PASS. Score bar and broadcast ticker remain visible and unclipped.
- **Pitch Corridor Clarity:** 100% PASS. No HUD elements occupy the central pitch between bowler and batsman.
- **Fail-Closed Responsiveness:** 100% PASS. Controls appear only during active 3D play and hide on 2D panels/menus.
