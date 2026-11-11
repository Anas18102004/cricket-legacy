# Cricket Legacy — Confirmed Mobile Control Decisions (Corrected)

## Context selection

- Controls are selected explicitly from the app-level HUD context picker. The saved batting style chooses Swipe or Classic Joystick; it does not auto-arm gameplay controls on entry to a 3D view.
- The renderer context is a secondary safety signal. A detected 2D menu hides the HUD and releases input. An open 3D renderer view alone cannot distinguish batting, bowling, fielding, replay, celebration, or another cutscene.
- Select **Hide controls** for 3D fielding, replays, celebrations, and unknown scenes. Automatic transitions remain disabled until a verified engine event exists.
- Fielding remains automatic; no fielder, catch, throw, or dive control is exposed.

## Batting timing

When the user selects a batting context, the batting controls remain available before delivery through the shot window. The host does not yet know when contact or ball-dead occurs, so it cannot automatically switch to Running or Ball Dead. Select Running explicitly when needed and Hide/Batting when the temporary interaction ends.

## Browser keyboard legends

The browser game's own keyboard guide (`D:/cricket07-browser-src/web/index.html`, Batting/Running/Bowling rows) documents:

| Context | Key | Browser help description | Native DirectInput scan code used by the host adapter |
|---|---|---|---|
| Batting | `S` | Normal shot | `DIK_S` `0x1F` |
| Batting | `Shift` | Loft modifier | `DIK_LSHIFT` `0x2A` |
| Batting | `D` | Advanced shot | `DIK_D` `0x20` |
| Batting | Arrows | Shot direction | Up `0xC8`, Down `0xD0`, Left `0xCB`, Right `0xCD` |
| Running | `D` | Take run | `DIK_D` `0x20` |
| Running | `A` | Cancel run | `DIK_A` `0x1E` |
| Bowling | `S` | Stock delivery | `DIK_S` `0x1F` |
| Bowling | `A`, `D`, `Shift`, `Alt` | Special delivery | `0x1E`, `0x20`, `0x2A`, `0x38` |
| Bowling | Arrows | Place ball | Same arrow scan codes as above |

These are browser help labels and host-side scan-code values; they do not prove that the current Android build's original Cricket 07 gameplay poll accepts or executes the same action. `D` and `S` have context-dependent meaning. The earlier `CANCEL=S`, `DEFEND=Enter`, and `W=bouncer` assumptions are unsupported by the browser keyboard guide and are not used in the revised HUD.

## Input evidence and acceptance

- Native tests use synthetic touch calls and verify key assertions/releases, joystick arrow sectors, deadzone, and hit bounds.
- Android runtime screenshots verify appearance only. A host `g_dik` bit or a render/device frame counter does not prove an original-game input poll or cricket action.
- Authentic batting, bowling, and running action acceptance remains **NOT PROVEN** pending observation of the corresponding original gameplay response.
