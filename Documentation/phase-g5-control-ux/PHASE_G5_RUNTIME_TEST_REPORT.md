# Phase G.5 — Runtime Test Report

## Environment

- Emulator: `emulator-5554`, Android x86_64
- Physical display setting: 1080×2400 portrait; game is landscape
- Activity/render surface: 2264×1017 pixels
- Native viewport query: `x=0, y=0, width=2264, height=1017`, window 2264×1017
- APK package: `org.cricketlegacy`

The surface fills the app content region in this observed configuration; Android system bars remain outside the game surface. This observation does not establish behavior on other aspect ratios or physical devices.

## Results

| Check | Expected | Result | Evidence |
|---|---|---|---|
| Main menu | No gameplay controls | PASS, visually observed | `artifacts/phase-g5-control-ux/main-menu-no-controls.png` |
| Team selection | No gameplay controls | PASS, visually observed | `artifacts/phase-g5-control-ux/team-select-no-controls.png` |
| Native viewport layout | Injected joystick/button bounds drive hit tests | PASS in native regression test | `test_mobile_input.c`, test 6 |
| Native input regression suite | Existing mode routing, release, multi-touch, cancellation and injected geometry remain correct | PASS, 7 groups reported by emulator-run harness | Emulator-run native test harness |
| Android build/install | APK builds and launches | PASS | Build/install command completed; app resumed on emulator |
| Classic joystick visual | Compact joystick and shot controls render | PARTIAL; visible in actual emulator capture, but scene is distorted/transitional | `artifacts/phase-g5-control-ux/classic-joystick-runtime-concept-context.png` |
| Swipe visual | No permanent joystick; transient gesture treatment | NOT VERIFIED in stable actual match frame | — |
| Bowling/Running/Fielding layouts | Correct context-specific controls | NOT VERIFIED in stable actual match frame | — |
| Touch down/up/cancel through final Android HUD | JNI delivery and balanced releases | PARTIAL; code path is instrumented, but no definitive final-build gameplay touch log tied to a visible active play context was captured | Source instrumentation; see limitation |
| Original Cricket 07 action acceptance | Intended batting/bowling/run action executes | **NOT TESTED** | No manual match evidence |
| Alternate aspect ratio / resizing | Layout remains inside resized viewport | NOT TESTED | Only 2264×1017 activity surface observed |
| Pause/resume and persistence | No stuck input; selected mode persists | NOT VERIFIED in this final run | Existing G.4 persistence remains; no new G.5 end-to-end result |
| Performance | No material regression | NOT TESTED | No before/after frame-time measurement |

## Evidence boundaries

Three distinct levels are reported:

1. **Android touch recognized:** the Activity/View receives a touch event.
2. **Input translated:** JNI/native input or host state receives the intended action and releases it correctly. Native harness tests verify host-side mapping; they do not prove a live match action.
3. **Original engine action accepted:** Cricket 07 performs the intended cricket action. **NOT TESTED.**

The native suite and screenshots cannot substitute for the third level. G.5 must not be described as proving that the controls work in Cricket 07 gameplay.

## Acceptance

The no-HUD menu requirement and injected geometry regression are demonstrated. Full acceptance remains **CONDITIONAL** pending stable actual-match evidence for the four relevant contexts, Android touch-to-native traces in active gameplay, screen-resize testing, and direct Cricket 07 action validation. Physical-device ergonomics remain outside this emulator-only milestone.
