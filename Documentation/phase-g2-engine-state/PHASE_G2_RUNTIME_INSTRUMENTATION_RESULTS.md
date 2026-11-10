# Runtime Instrumentation Results

## Probe

A temporary read-only diagnostic sampled the renderer context getter every 60 Java frame callbacks. It read the existing renderer classification and engine frame counter; it did not write match state. Diagnostic source logging was removed after sampling. The reusable getter remains fail-closed and lifecycle invalidation remains enabled.

## Observation

The emulator (`emulator-5554`, x86_64) reported known menu-like frames during startup/front-end progression and open-play frames after entering the 3D ground. Earlier captured samples showed the flag returned to menu-like during part of the scripted bowling run-up and returned to open-play near a scripted release milestone. The Phase F sequence is automated harness progression; these are not manual user gameplay observations, nor proof that the named milestones match original engine states.

The runtime sampling from this instrumented build produced values (0/1) only; it did not inspect private memory, dump guest sections, or infer game state. A clean-source APK was subsequently rebuilt for both ABIs after removal of the temporary log.

## Interpretation

The reader distinguishes renderer contexts only. It is unsuitable for specific HUD state decisions. Detailed states remain UNKNOWN. No runtime pass is claimed for batting, bowling, running, fielding, pause transitions, or persistence.
