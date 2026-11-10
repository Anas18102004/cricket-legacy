# Phase G.1 Current HUD Problem Audit

**Baseline:** Phase G commit `4dc5efaad8b3b306f6061ff742fb7e9d126b3ec4`.  
**Branch:** `phase-g1-control-ux-correction`.

## Screenshot findings

Reviewed the actual Phase G emulator capture at `Documentation/artifacts/phase-g-codex-validation/emulator-runtime.png`.

- The joystick, five batting/run buttons, and permanent mode pill draw over the match view at the same time.
- RUN/CANCEL appear in a dead-ball/ready scene; no phase condition controls their visibility.
- Bowling controls are absent from the Java view even though native button records exist.
- The Java overlay and native hit rectangles are computed separately with different dimensions/scales, so visible controls and native touch targets need not coincide.
- Controls are positioned against the Android view dimensions, while the game content has letterbox/pillarbox margins; they can intrude into unused margins or cover gameplay.
- The permanent `HYBRID CONTROLS` pill consumes screen space and is not a Settings screen.

## Correction made

The HUD now starts hidden, including joystick, buttons, swipe trails and mode indicator. Its touch layer is `GONE`, and the underlying `SurfaceView` receives touch events for the original game menu bridge. Native mobile input also starts in `GAME_PHASE_MENU` and ignores mobile control gestures unless an explicit batting phase is set. The current host has no trustworthy phase source, so the runtime remains safely hidden in both the splash and match captures.

## Remaining

No active joystick/button redesign is enabled. It would be misleading to tune controls against a guessed viewport or display them over unknown scenes. The source screenshot [runtime-hidden-hud.png](../artifacts/phase-g1-control-ux/runtime-hidden-hud.png) shows the original splash without mobile controls. [runtime-current-scene.png](../artifacts/phase-g1-control-ux/runtime-current-scene.png) shows a match scene also without them.
