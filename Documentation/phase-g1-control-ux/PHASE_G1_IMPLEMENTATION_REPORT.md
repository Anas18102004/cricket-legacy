# Phase G.1 Implementation Report

## Changes

- `mobile_input.c`: initial phase changed from BATTING to MENU; invalid phases fall back to MENU. Native mobile gestures are ignored unless phase is explicitly BATTING. Leaving batting clears stroke keys, toggle/button state, pointers, joystick axes and gamepad state.
- `MobileHudOverlayView.java`: added a visibility/reset API for fail-closed control presentation.
- `MainActivity.java`: hides the gameplay overlay at startup and attaches its existing touch listener to the underlying SurfaceView so original menu taps still reach the game when the overlay is gone.
- `test_mobile_input.c`: test setup explicitly opts into batting; added a menu-context swipe assertion that confirms no mobile shot is emitted.

## Not implemented

No trustworthy native state observer, Settings integration, persistent control preference, actual bowling/run phase mapping, joystick visual redesign, or runtime control-layout activation was available. These remain blockers, rather than guessed implementations.

## Build

Native `cricket_native_harness` rebuilt successfully for x86_64 and arm64-v8a via the existing CMake trees. Updated libraries were packaged into the APK by `scripts/build_apk.ps1`. Java compilation emitted only its existing deprecated-API note. The updated APK installed and launched on the x86_64 emulator.
