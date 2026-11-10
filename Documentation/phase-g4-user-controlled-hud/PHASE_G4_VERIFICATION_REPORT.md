# Phase G.4 Verification Report

## Three distinct verification levels

| Level | Evidence | Result |
|---|---|---|
| 1. Touch recognized | Android selector screenshots and native touch-handler tests for down/move/up/cancel | PASS for UI selection flow and handler-level touches; not a manually played match |
| 2. Input translated | Native tests with mock host functions assert candidate DIK/gamepad states for swipe, joystick, bowling, and Running contexts | PASS for mobile-layer translation into the host shim in tested cases |
| 3. Original game accepted input and performed the intended cricket action | No reliable action-acceptance hook or human-played match evidence | NOT TESTED |

Passing levels 1 and 2 does not establish level 3. Action bindings remain candidates pending G.5 authentic gameplay validation.

## Native input results

`cricket_mobile_input_tests` ran on the x86_64 emulator with C assertions explicitly enabled. Passing cases: menu closed by default; eight swipe sectors and balanced pulse release; classic joystick deadzone/axis release; multi-touch joystick plus loft; classic mode ignores unrelated swipe; Running-context candidate RUN down/up; Bowling-context candidate standard-bowl down/up; menu context ignores controls; ACTION_CANCEL and context changes release held input.

These deterministic tests use mocked host input. They do not execute Cricket 07's action parser.

## Android runtime visual checks

Screenshots are in `Documentation/artifacts/phase-g4-user-controlled-hud/`:

- `g4-stadium-selection-hidden.png` and `g4-team-select-controls-hidden.png`: original front-end screens remain clear.
- `g4-context-selector.png` and `g4-context-selector-open-play.png`: the user context selector is reachable with Android Back.
- `g4-active-classic-joystick-final.png`: Classic Joystick layout is user-activated in a real renderer frame. The joystick and action cluster were repositioned above the bottom score area. The match itself was advanced by the existing Phase F script, not played manually.
- `g4-cutscene-no-controls.png` / `g4-cutscene-swipe-context-suppressed.png`: menu-like/cutscene renderer context hides the HUD and requires a new user selection.

An active Swipe screenshot for a stable open-play frame and Bowling/Running HUD screenshots were not captured. `g4-batting-classic-joystick.png` predates the final position correction and is explicitly not acceptance evidence.

## Build and launch

x86_64 and arm64-v8a native libraries compiled, the debug APK packaged, and the APK installed/launched on the x86_64 emulator. Native harness initialization succeeded without observed JNI load/startup exception. `git diff --check` passed.
