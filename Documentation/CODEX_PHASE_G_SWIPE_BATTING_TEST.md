# Codex Phase G Swipe Batting Test

**Overall status: PARTIAL**

- **PASS (runtime ingress only):** On the x86_64 emulator, an ADB swipe during a real match produced `[Stroke Dispatched] Sector=7 Angle=45.0 deg Dist=268.7 px Vel=2.86 Loft=1`. The trace is [emulator-touch-trace.txt](artifacts/phase-g-codex-validation/emulator-touch-trace.txt).
- **PASS (unit scope):** The eight direction classifications and mocked key pulse assertions passed in [automated-test-output.txt](artifacts/phase-g-codex-validation/automated-test-output.txt).
- **NOT TESTED:** Resulting in-game shot, timing against a delivered ball, slow/fast and short/long swipe comparison, ground-versus-loft outcome, and batting behavior in each of the four named modes.
- **Source finding:** `mobile_input.c` stores `s_config.batting_mode` but never branches on it. All modes share the same touch path. Velocity is calculated from total displacement / elapsed time and only selects loft above a fixed threshold; no ground/loft gameplay outcome was established.
- **Source finding:** Several named sectors collapse to the same arrow combination (`MID_WICKET` and `MID_ON` both Up+Right). A directional keyboard event does not establish eight distinct cricket shot types.

The screenshot [emulator-runtime.png](artifacts/phase-g-codex-validation/emulator-runtime.png) shows the running match and HUD. A dispatched event is not evidence that the batsman played the intended shot.
