# Phase G.1 Final Acceptance Report

## REJECTED — safe HUD correction implemented; contextual control system incomplete

The visible-control defect is corrected in the tested build: the gameplay overlay and mobile gesture path fail closed, while menu touch routing remains attached to the original surface bridge. Actual splash and match screenshots show no gameplay controls. Native input and Java were rebuilt and the updated APK installed.

The requested professional gameplay system is not accepted because the host has no reliable Cricket 07 phase observer. Batting modes cannot safely be enabled, running/bowling contexts cannot be identified, and no existing Settings integration or preference persistence path was found. Guessing these states from `s_state_tick` would repeat the exact gameplay-context defect this phase is intended to correct.

### Acceptance summary

- No controls on splash: PASS.
- No controls on match scene while state is unknown: PASS (fail-closed behavior).
- Original menu touch delivery: PARTIAL (source wiring verified; controlled navigation assertion not captured).
- Swipe, Classic, Pro, Hybrid actual match gameplay: BLOCKED.
- Contextual RUN/CANCEL, bowling: BLOCKED.
- Automatic fielding unchanged by source edits: PASS; actual fielding scenario NOT TESTED.
- Settings integration/persistence: BLOCKED.
- Emulator build/install: PASS.
- ARM64 runtime and physical-device validation: NOT TESTED.

### Required next dependency

Identify and validate a stable state signal in the translated Cricket 07 engine for menu, batting, bowling, ball-in-play/running and ball-dead states; then connect that signal to the HUD and Settings preference lifecycle. Until that evidence exists, hidden controls are the only safe runtime behavior.
