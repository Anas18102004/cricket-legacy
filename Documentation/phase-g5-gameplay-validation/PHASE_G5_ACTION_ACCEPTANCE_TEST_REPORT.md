# Phase G.5/G.6 Action Acceptance Evidence Re-audit

## Corrected verdict

**Level 3 — NOT PROVEN.** The earlier version of this report incorrectly called host `g_dik` state transitions proof that Cricket 07 accepted and executed an action. `nativeCricketRenderFrame()` logged rising edges in the host DirectInput array; it did not instrument the original game's input poll or observe a batting/bowling animation. The former `LEVEL3_ENGINE_INPUT_ASSERTED` log name has been replaced by `HOST_DIK_STATE_RISE` to make that limit explicit.

The previously recorded six-frame hold is a mobile adapter callback count, not a verified count of original engine sampling frames. It does not guarantee game-thread sampling.

## What has been verified

| Level | Result | Evidence and limit |
|---|---|---|
| Android `MotionEvent` ingress | **NOT TESTED for this revision** | Runtime HUD screenshots show controls, but no Android touch was exercised during this validation pass. |
| Native host translation | **PASS in focused native tests; runtime path NOT TESTED** | `cricket_mobile_input_tests` on `emulator-5554` verifies scan-code transitions, releases, directions, deadzone, and contextual hit routing using synthetic native touch events. This does not itself verify Java MotionEvent/JNI delivery. |
| Authentic Cricket 07 action | **NOT PROVEN** | No recorded batting animation, bowler release, run completion/cancellation, ball trajectory, or score change is attributable to a controlled touch. |

## Browser keyboard mapping source

`D:/cricket07-browser-src/web/index.html` documents batting `S` (normal shot), `Shift` (loft modifier), `D` (advanced shot), arrow directions, running `D`/`A`, bowling stock `S`, special delivery keys `A`/`D`/`Shift`/`Alt`, and arrow placement. `loader.js` maps the in-play stick to arrow key states and releases held keys on pointer cancellation, page blur, and visibility loss.

These sources support the displayed keyboard legends and host scan-code candidates. They do not prove how the current Android build's original game loop interprets each key in a live match. The conflicting earlier `CANCEL=S` and `DEFEND=Enter` assumptions are not used by the revised HUD.

## Historical evidence limitation

The screenshots and telemetry cited in the former report came from an older control build and do not establish action acceptance. They are retained as historical artifacts only; the G.6 runtime captures and focused-test results are documented separately.
