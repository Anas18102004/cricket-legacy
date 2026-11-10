# Codex Phase G Automated Test Results

**Status:** PARTIAL. The existing `test_mobile_input.c` test program was compiled with NDK Clang for Android x86_64, pushed to the emulator, and executed. Full output is in [automated-test-output.txt](artifacts/phase-g-codex-validation/automated-test-output.txt).

| Test | Status | Result / limit |
|---|---|---|
| Eight swipe-sector classifications | PASS | All eight expected enum sectors matched. This checks classification only. |
| Direction-key and Enter pulse / release | PASS | Mock input observed expected key-down and release after six calls to frame tick. |
| Joystick deadzone, scaling, release | PASS | 5 px deflection yielded zero; 60 px yielded LX 19917; release centered axes. |
| One three-pointer joystick + loft + swipe scenario | PASS | Mock confirmed concurrent states and loft inheritance. |
| Actual simultaneous button ownership / independent releases | NOT TESTED | Existing test has only one button plus joystick/swipe; it does not expose the release-all-buttons defect. |
| ACTION_CANCEL with multiple active pointers | NOT TESTED | Not covered by the test source. Source inspection indicates only the pointer ID passed with CANCEL is released. |
| Four batting modes, timing/footwork, velocity behavior | NOT TESTED | Unit test does not configure and exercise each mode or connect to gameplay results. |
| Bowling, running, fielding, pause/resume, orientation | NOT TESTED | No such scenarios in this test executable. |
| Left-handed layout, opacity/scale settings, touch outside bounds | NOT TESTED | No assertions cover these. |

The test harness uses mock `input_key_dik` and `input_gamepad` implementations. Its PASS results establish native recognizer outputs, not original-engine action semantics or successful cricket shots.
