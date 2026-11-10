# Phase G.5 — Mobile Controls UX Implementation Report

## Scope

This milestone refines the existing user-selected HUD without changing Cricket 07 menus, physics, fielding AI, or gameplay algorithms. The HUD remains user-contextual as in G.4; renderer suppression is fail-closed. No timing heuristic was added to guess batting, bowling, or ball state.

## Changes

- `MobileHudOverlayView.java` now derives compact controls from the native-rendered viewport, scales dimensions with Android density, supports handedness, and draws restrained translucent joystick/action controls. Swipe feedback is transient and controls are suppressed when the renderer context is hidden.
- Android sends the computed joystick center/radius and button rectangles through `nativeCricketSetHudLayout`. Native drawing ownership and touch hit testing therefore use the same viewport-derived geometry instead of independent fixed positions.
- The Android `MotionEvent` path keeps non-gameplay surface touches distinct, releases held state on cancellation/context/layout changes, and forwards touch events through JNI. Pause/resume configuration re-applies current HUD geometry.
- Added a native regression case for injected Android viewport geometry. Existing touch mapping and automatic fielding behavior remain intact; no manual fielding control was introduced.

## Build

The Android APK was built and installed on emulator `emulator-5554` (package `org.cricketlegacy`). Native targets were built for `x86_64` and `arm64-v8a`. The emulator is x86_64. Build output contains an existing deprecated-API warning in `MainActivity`; no build failure occurred.

## Visual evidence

See `Documentation/artifacts/phase-g5-control-ux/`. The main menu and team-selection captures show no gameplay HUD. A joystick-layout capture shows the revised compact HUD over a real emulator render, but the underlying 3D frame is visibly distorted/transitional; it is layout evidence only, not an acceptable gameplay-quality screen or proof of a cricket action. The emulator did not provide a stable, manually played open-play match during this run, so G.5 visual acceptance remains conditional.

## Limitations

- This milestone does not establish that the original Cricket 07 engine accepted or executed batting, bowling, or running actions.
- Manual match tracing and physical-device ergonomics were deferred by the user.
- Bowling, Running, Swipe, and Fielding were not each captured in a stable actual-match frame during this final build run.
- No performance comparison was measured; no FPS claim is made.

## Verdict

**CONDITIONALLY APPROVED for the viewport-aligned Android overlay and native input bridge.** Actual Cricket 07 action acceptance is **NOT TESTED** and full G.5 gameplay UX acceptance is not established.
