# Codex Phase G Screenshot and Log Evidence

Evidence is stored in the new directory `Documentation/artifacts/phase-g-codex-validation/`.

| Artifact | What it demonstrates |
|---|---|
| `emulator-runtime.png` | Emulator screenshot of the running match scene and visible HUD. Captured with `adb shell screencap` and `adb pull`. |
| `emulator-runtime-log.txt` | Selected recent native/app telemetry from logcat, including FPS and RSS values. |
| `emulator-touch-trace.txt` | Recent telemetry and the ADB swipe's native stroke-dispatch record. |
| `emulator-meminfo.txt` | One `dumpsys meminfo` snapshot. |
| `emulator-gfx-framestats.txt` | Android graphics frame stats snapshot; not a controlled game-engine benchmark. |
| `automated-test-output.txt` | Output from the x86_64 Android executable built from `mobile_input.c` and `test_mobile_input.c`. |
| `test_mobile_input` | Reproducible x86_64 Android test executable pushed to `/data/local/tmp/test_mobile_input` for execution. |

The source and unit-test artifacts do not show that all mobile controls produced correct cricket gameplay. No physical-device screenshot exists.
