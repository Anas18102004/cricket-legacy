# Automatic Fielding Preservation

RTTI lists `CFielderManager`, `CCatchBehaviour`, `CKeeperBehaviour`, and related behavior classes. Function metadata shows entries for these types, including `CFielderManager` and `CCatchBehaviour` virtual functions. That establishes relevant engine components exist, but not the full movement/catch/pickup/throw call graph or runtime behavior.

No G.2 change calls these functions, writes fielding state, changes gameplay algorithms, or adds manual fielding input. The G.1 no-manual-fielding posture is preserved. This research did not perform a controlled runtime catch test, so preservation is source-scope evidence, not a gameplay acceptance test.

Before exposing any fielding action, trace the original event and action path and prove it does not override automatic decisions. Default mobile HUD should continue to have no fielder joystick, catch, throw, or selection control.
