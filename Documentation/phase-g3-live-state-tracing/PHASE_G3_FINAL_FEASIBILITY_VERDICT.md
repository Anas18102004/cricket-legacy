# Final Feasibility Verdict

## Verdict: NOT PROVEN

No authentic original gameplay state or game-side action acceptance transition was proven in G.3. Manual gameplay was not run at the user's request; every requested match scenario is NOT TESTED. Scripted harness progression is explicitly excluded as evidence.

Useful progress: an opt-in manual diagnostic build mode is prepared. It compiles out frame-triggered scripted inputs and records a bounded touch timeline plus the renderer's coarse context. Host source inspection verifies DIK down/up propagation into DirectInput state and Windows-style messages. Generated function/RTTI leads were inspected, but not enough to establish behavior or live state fields.

No controls were exposed, no original menu was changed, no gameplay physics/AI was modified, and the unsupported `0x008A1000` candidate remains rejected. G.1/G.2 changes remain in the worktree. Next evidence requires a human-operated diagnostic match and targeted function instrumentation based on verified call contracts.
