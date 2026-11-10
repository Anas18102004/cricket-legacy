# Codex Phase G Android Runtime Test

**Overall status: PARTIAL**

- **PASS:** APK installed and launched on the connected x86_64 emulator.
- **PASS:** Logcat showed both native libraries loaded, native harness initialization succeeded, PE image load succeeded, and authentic game thread started.
- **PASS (visual):** [emulator-runtime.png](artifacts/phase-g-codex-validation/emulator-runtime.png) shows an in-match Cricket 07 scene with the mobile HUD.
- **PARTIAL:** A synthetic ADB swipe event was received by the real app and generated a native stroke-dispatch log. The actual cricket response was not confirmed.
- **NOT TESTED:** Manual menu navigation, controlled match start through on-screen controls, each batting mode's gameplay, bowling, RUN/CANCEL, fielding/catching, lifecycle pause/resume, rotation, repeat restarts, long-run stability, ANR/crash stress, and physical-device behavior.
- **Physical ARM64 device:** NOT TESTED; none connected.

The six-frame input test was run separately with mocks and does not upgrade runtime gameplay status.
