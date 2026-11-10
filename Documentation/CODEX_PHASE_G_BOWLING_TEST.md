# Codex Phase G Bowling Test

**Overall status: NOT TESTED (gameplay)**

- **PASS (build/runtime prerequisites):** APK installed and authentic engine initialization reported success on the x86_64 emulator.
- **NOT TESTED:** Selecting delivery type, pitch target, run-up, release timing, swing, cut, bouncer/slower delivery, or resulting ball trajectory in a match.
- **Source finding:** `MobileHudOverlayView.java` constructs only LOFT, ADVANCE, DEFEND, RUN, and CANCEL buttons. It draws no bowling delivery controls or pitch marker. `mobile_input.c` initializes bowling button state records but the Java HUD does not render them.
- **Source finding:** `GAME_PHASE_BOWLING` is not used to select or gate actions in `mobile_input.c`. `nativeCricketSetGamePhase` is exposed but no call site was found in `MainActivity.java` or the view.
- **Source finding:** The same `DIK_D` is assigned to RUN and CUT; the same `DIK_S` to CANCEL and STANDARD. With no phase gate, context-sensitive semantics are not enforced by this layer.

The six-frame automated test concerns batting input only. It is not bowling evidence.
