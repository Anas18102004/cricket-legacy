# Live Gameplay Observation

## Status: NOT TESTED

The requested human-operated scenarios were not performed in this session. The user asked that gameplay be marked NOT TESTED and instrumentation prepared for a later operator session. No gameplay screenshot, video, or trace is claimed as evidence.

## Critical baseline finding

`android-native-prototype/src/main.c`, `nativeCricketRenderFrame`, contains the Phase F frame-count progression script. It injects Enter, Space, arrows, gamepad buttons and axes at fixed ticks, including labeled bowling and batting milestones. Therefore runs of that default build are scripted harness progression, not independent human gameplay. They cannot validate manual transition behavior.

An opt-in build mode `CRICKET_G3_MANUAL_TRACE=ON` is now available in CMake. It compiles out that progression script and records operator touch events in a fixed 128-event buffer. Default builds retain existing behavior. This mode records touch events and the coarse renderer class only; it does not yet trace original gameplay functions or guest variables.

## Scenario status

| Scenario | Status | Evidence |
|---|---|---|
| Menu to match | NOT TESTED | Requires operator-driven diagnostic session |
| Bowling delivery | NOT TESTED | No manually labeled delivery |
| Batting shot | NOT TESTED | No manually labeled shot |
| Running/cancel | NOT TESTED | No manual run attempt captured |
| Ball dead | NOT TESTED | No dead-ball transition trace |
| Fielding/catching | NOT TESTED | No manual gameplay capture |
| Wicket/over/innings/pause/end | NOT TESTED | No manual capture |

See `Documentation/artifacts/phase-g3-live-state-tracing/README.md` for trace format and evidence status.
