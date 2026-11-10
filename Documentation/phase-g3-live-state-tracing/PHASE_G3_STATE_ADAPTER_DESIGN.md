# State Adapter Design

## Recommendation: fail closed

Keep G.1 gameplay HUD hidden. Keep the G.2 renderer adapter as a secondary rendering-context hint only. It must not select batting, bowling, running, or fielding controls.

Future adapter proposal:

- `state = UNKNOWN` by default and on lifecycle interruption, stale sample, unknown enum, missing match/player object, or contradictory evidence.
- Separate verified context, control ownership (user/AI), delivery stage, ball-live/dead phase, running/cancel availability, and exceptional pause/wicket/over/innings flags rather than guessing a single mode from rendering.
- Publish a versioned immutable snapshot from the game thread, read-only on Android; avoid raw guest pointers over JNI.
- Permit controls only when every required gate is positively verified. Unknown hides controls.

No adapter for original gameplay state was implemented in G.3. Current diagnostic mode reports touch events and coarse renderer context only.
