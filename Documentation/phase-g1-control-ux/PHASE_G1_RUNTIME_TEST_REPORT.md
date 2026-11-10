# Phase G.1 Runtime Test Report

**Device:** `sdk_gphone64_x86_64`, emulator 5554. Physical device/ARM64 runtime: NOT TESTED.

| Scenario | Status | Evidence |
|---|---|---|
| APK install and launch | PASS | Updated APK installed; native app process started. |
| Splash scene has no gameplay controls | PASS | [runtime-hidden-hud.png](../artifacts/phase-g1-control-ux/runtime-hidden-hud.png) shows original Cricket 07 splash, no joystick/buttons/pill. |
| Match scene has no gameplay controls | PASS | [runtime-current-scene.png](../artifacts/phase-g1-control-ux/runtime-current-scene.png) shows a match scene, no gameplay overlay. |
| Menu touch remains routed to native bridge | PARTIAL | SurfaceView now owns the listener and native bridge still invokes the original tap path. No controlled menu-navigation assertion was captured. |
| Native menu phase rejects a swipe | PASS | x86_64 NDK-built test output in [automated-test-output.txt](../artifacts/phase-g1-control-ux/automated-test-output.txt). |
| Actual batting controls | BLOCKED | No trustworthy state publisher enables batting; controls remain hidden. |
| Settings mode persistence, running, bowling, fielding | NOT TESTED | No integrated state/settings implementation to exercise. |

These screenshots are actual emulator captures. No concept mockups are represented as runtime output.
