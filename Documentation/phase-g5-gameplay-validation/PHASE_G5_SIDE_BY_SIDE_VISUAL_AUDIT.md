# Phase G.5/G.6 Runtime Visual Evidence Re-audit

## Current build captures

Raw emulator captures from the revised build are in [`Documentation/artifacts/phase-g6-contextual-hud/`](../artifacts/phase-g6-contextual-hud/). They are actual `adb screencap` output, not concept art.

| Capture | What it shows | What it does not establish |
|---|---|---|
| `01-default-context.png` | Original profile prompt with zero gameplay HUD. | Other menu layouts or input behavior. |
| `02-main-menu.png` | Original stadium-selection UI without gameplay controls. | Selection/touch usability beyond the successful tap used to navigate. |
| `05-batting-classic.png` | 3D match rendering with manually selected Classic Joystick controls and Cricket 07's original broadcast HUD. | That the selected manual context matches the human player's team or that batting input was accepted. This capture predates the final circular-button polish. |
| `06-batting-swipe.png` | 3D render with swipe mode selected; no permanent joystick. | It was captured during a later close-up/transition, not during a verified in-flight swipe. |
| `07-bowling.png` | Bowling control group rendered in a 3D presentation. | The scene is a wicket celebration, not a verified bowling setup/run-up/release. |
| `08-running.png` | RUN/CANCEL group rendered over a later match presentation. | The scene is a result/transition, not active running. This demonstrates the limit of manually selected context on 3D scenes. |
| `09-fielding.png` | Fielding context hides the mobile HUD. | Automatic fielding/catching correctness. |

The screenshots were captured at emulator output 2400×1080 (game render window reported 2264×1017, with black margins/navigation area). They are visual snapshots of selected app-level contexts; the app has no verified batter/bowler/ball-phase signal.

## Revised HUD layout

The Android overlay now uses circular key buttons, compact three-key batting layout, five bowling key/modifier buttons, an arrow-marked joystick, and the browser keyboard legends. The joystick and button centers are computed from the renderer viewport bounds supplied by JNI, not fixed screenshot pixels. Menu, fielding, replay, and Hide contexts suppress gameplay controls. Swipe mode omits the permanent joystick.

## Acceptance status

The earlier table's 100% pitch/scoreboard and Level 3 claims are withdrawn. Visual captures support only the visible layout and fail-closed behavior observed in those individual frames. Actual touch routing and Cricket 07 action acceptance remain separate checks; see the corrected action report.
