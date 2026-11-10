# Phase G.4 User-Controlled HUD

## Result

Implemented explicit user-selected HUD contexts with fail-closed renderer gating. Controls start hidden on every app launch. Android Back opens an app-level context selector; Swipe/Classic Joystick preference is saved in SharedPreferences. The original Cricket 07 Settings menu was not modified.

The renderer signal is only a secondary gate: a known open-3D context is required to show the user-requested layout. Menu-like/cutscene frames clear the active context. The renderer does not choose batting/bowling/running or prove the user's selected context matches Cricket 07. Users must reselect after returning to open play.

## Context choices

- Hide controls (default).
- Batting - Swipe.
- Batting - Classic Joystick.
- Bowling controls.
- Running - RUN / CANCEL.
- Ball dead - batting controls only.
- Fielding - automatic, no controls.
- Replay - hide controls.

RUN/CANCEL render only in Running context. Fielding and Replay show no mobile controls. Ball dead is selected manually to hide running actions. No automatic ball-dead detection is claimed.

## Input ownership and reset

`nativeCricketOnTouch` routes original surface taps only to the original mouse/menu path. Visible HUD touches use `nativeCricketOnHudTouch`, preventing button taps from also activating the underlying game. Context changes and ACTION_CANCEL clear held keys, gamepad axes, button toggles, and pointer ownership.

## Verification boundary

The Android selector, hidden menu state, and active Classic Joystick layout were observed at runtime. Native C tests verify mobile touch to host shim translation. The tested match presentation was driven by scripted harness inputs. Original Cricket 07 action acceptance remains NOT TESTED.
