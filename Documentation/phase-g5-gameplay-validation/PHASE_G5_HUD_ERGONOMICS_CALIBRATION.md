# Phase G.5 — HUD Ergonomics Calibration

## Reference and design decision

The supplied Real Cricket screenshot and current Real Cricket captures are treated as layout references only: directional input at lower left, compact action cluster at right, central pitch left open, and the original broadcast display retained. Real Cricket’s official help page confirms that users can select a control style from Gameplay Settings; it does not establish Cricket 07 action mappings. [Real Cricket gameplay settings](https://help.cookierunindia.krafton.com/hc/en/10-realcricket/faq/184-how-do-i-change-gameplay-settings/) and [a Real Cricket batting screenshot](https://com-nautilus-realcricket.en.uptodown.com/android) are the references used here.

No Real Cricket-only action such as Push, Leave, Front Foot, or Back Foot was added. Cricket 07's existing mobile actions remain LOFT, ADVANCE, DEFEND, RUN, CANCEL and its existing bowling buttons.

## Emulator geometry

Measured on `emulator-5554`:

- Android display: 1080×2400 portrait, 420 dpi (density 2.625)
- Landscape activity/overlay: 2264×1017 px
- Native renderer viewport: x=0, y=0, w=2264, h=1017
- Viewport center/pitch axis reference: x=1132 px

The Android overlay sends this geometry to native input, so visible bounds and hit rectangles share the same coordinate system.

## Calibrated right-handed batting layout

Values are calculated from the current overlay formulas at this emulator size; they are not pixel measurements from a stable match screenshot.

| Element | Position/bounds in viewport px | Approx. dp |
|---|---:|---:|
| Joystick center | (339.6, 630.5) | (129.4, 240.2) |
| Joystick radius | 162.7 px, clamped by 16% of viewport short side | 62.0 dp effective; requested 84 dp is capped to fit viewport |
| Joystick visible bounds | x=176.9–502.3, y=467.8–793.2 | 67.4 px minimum left inset |
| Action button size | 132.2×132.2 px | 50.4 dp; exceeds 48 dp target |
| Button gap | 31.5 px | 12 dp |
| Right cluster bounds | x=1905.1–2201.0 px | 24 dp right inset |
| Cluster first row | y=488.2–620.4 px | 48% viewport top anchor |
| Cluster second row | y=651.9–784.1 px | 64.1% viewport bottom |
| RUN/CANCEL size | 126.0 px | 48 dp minimum target |

The joystick has a 15% radial deadzone and linear response beyond it. It is anchored at 15% viewport width and 62% viewport height. The control group is kept below the upper play area and above a reserved lower band; the pitch centerline remains unobstructed by these rectangles. Left-handed layout mirrors the sides.

## Clear-zone and scoreboard caveat

The user-provided reference has its broadcast ticker at the bottom. This build does not expose the original scoreboard's rectangle as machine-readable geometry, and the emulator could not reach a stable match through Team Select. Therefore the requested `>32 dp` clearance from the *actual* scoreboard has **not been runtime-measured**. The geometry reserves approximately the bottom 20% for original lower HUD content, but that is only a layout safeguard, not proof of clearance from the scoreboard in every camera/aspect ratio.

## Touch and context rules

- Batting: one joystick plus LOFT/ADVANCE/DEFEND in Classic, or no joystick plus the same buttons and an in-flight swipe vector/sector label in Swipe.
- Swipe feedback is drawn only while the pointer is moving and disappears on release/cancel.
- RUN/CANCEL stay limited to the explicitly selected Running context.
- Menus remain fail-closed. No fielding controls were added.
- Existing bindings in the mobile shim: LOFT `DIK_LSHIFT` (0x2A), ADVANCE `DIK_LCONTROL` (0x1D), DEFEND `DIK_RETURN` (0x1C), RUN `DIK_D` (0x20), CANCEL `DIK_S` (0x1F). These are shim bindings; Cricket 07's acceptance of each is not established by this report.
