# Phase G.4 Known Limitations and G.5 Handoff

- Context is manually selected. Users choose the current batting/bowling/running context and switch to Ball dead or Hide to remove temporary run actions.
- The renderer is only a coarse secondary gate. Menu-like/cutscene frames clear the selected context; some valid open-play HUD moments may be hidden. It is not a full menu/game detector.
- Shot, bowling, RUN, and CANCEL bindings are still unverified candidates. The tests prove only touch-to-host-input translation.
- The Android Back selector is an app-level settings extension. The original Cricket 07 Settings menu was not modified.
- Active renderer screenshots were taken during the existing scripted Phase F progression. No manual Cricket 07 action acceptance was observed.
- Fielding context shows no manual controls and leaves automatic fielding code untouched.
- Runtime presentation evidence includes the final Classic Joystick layout. Stable Swipe, Bowling, and Running screenshots are still missing.

## G.5

Validate Swipe, Classic Joystick, bowling, RUN, and CANCEL with an operator-controlled match. Record separately: Android touch recognized; host input state delivered; and original game action accepted. Keep Option B function tracing exploratory until entry points and semantics are verified.
