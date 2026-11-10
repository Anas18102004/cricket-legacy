# Phase G.3 diagnostic artifacts

No gameplay screenshots or traces are present: manual scenarios were not performed. Do not interpret prior G.2 renderer samples or Phase F screenshots as G.3 gameplay evidence.

The opt-in `CRICKET_G3_MANUAL_TRACE` diagnostic emits logcat records with tag `CricketLegacy-Main` and prefix `[G3_TOUCH]`. Each line includes sequence, callback tick, Android action, pointer id, screen coordinates, and renderer context. The fixed ring holds at most 128 events; overflow is reported. Capture only during an operator-labeled manual session, and pair trace timestamps with a real screen recording. The diagnostic does not record function calls or guest memory.
