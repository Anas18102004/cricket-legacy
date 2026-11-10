# Codex Phase G Final QA Verdict

## REJECTED

The APK packages and installs on the x86_64 emulator, the existing mock-backed input tests pass, and the authentic engine renders a match scene. These results do not satisfy Phase G acceptance criteria.

### Blocking findings

1. **Four batting modes are not independently implemented in the native input path.** `s_config.batting_mode` is stored but not read after configuration; the same gesture and joystick behavior is used regardless of selected mode. Pro crease movement is not established.
2. **Bowling HUD/runtime integration is incomplete in inspected code.** The Android view renders no delivery buttons or pitch targeting UI; native phase state does not gate controls, and no gameplay code calls the exposed phase setter.
3. **Contextual running and cancellation are not established.** Buttons are always rendered. `RUN`/`CUT` both map to `DIK_D`, `CANCEL`/`STANDARD` both to `DIK_S`, without phase-dependent routing in this input layer.
4. **Pointer reliability gaps remain.** Non-toggle button release clears every pressed non-toggle control, and ACTION_CANCEL handles only the supplied pointer ID. Existing tests do not exercise these cases.
5. **Gameplay and fielding acceptance tests are missing.** No successful shot, bowling delivery, run/cancel, catch, or fielding behavior was confirmed. No physical ARM64 test was possible.
6. **Performance validation is incomplete.** The short emulator samples vary from about 17 to 44 FPS and RSS rises during capture; there is no controlled baseline comparison.

### Test class separation

- **Automated:** PASS for three existing mock-backed groups (eight sector classifications and pulse release, joystick scaling/deadzone/release, one multi-touch scenario). Not a complete reliability suite.
- **Emulator:** PARTIAL. APK installed, authentic engine rendered a match and received a swipe event; resulting gameplay action is unverified.
- **Physical device:** NOT TESTED.
- **ARM64 runtime:** NOT TESTED.

Phase G fails the required criteria that all four batting modes work in match gameplay, bowling and running/cancel work through the original engine, automatic fielding/catching are verified, critical pointer defects are cleared, and reproducible performance evidence exists. Do not treat this verdict as approval to progress toward release.
