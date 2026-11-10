# Phase G.1 Contextual HUD Architecture

## Current safe behavior

1. Android activity creates the authentic game surface.
2. `MobileHudOverlayView` is initialized hidden (`View.GONE`) because no authoritative gameplay-state callback exists.
3. The underlying `SurfaceView` has the touch listener. Menu touches continue through `nativeCricketOnTouch` to the original touch-to-mouse/cursor bridge.
4. Native mobile input starts in `GAME_PHASE_MENU`; mobile control gestures are ignored unless native phase is explicitly set to batting.
5. Leaving batting for any other explicitly supplied phase clears active mobile keys, buttons, pointers and axes.

## Required future flow

An engine-owned, verified state observer must publish state transitions to the host. The host can then set a typed HUD context and the view can render only controls valid for that state. Unknown/unrecognized transitions map to hidden. The frame-count progression script in `main.c` must never drive this state machine.

No engine observer or validated state offsets were found. The current build therefore does not expose a method that guesses or manually forces a context in production.
