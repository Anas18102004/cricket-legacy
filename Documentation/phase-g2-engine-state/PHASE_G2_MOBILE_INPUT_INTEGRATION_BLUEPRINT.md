# Mobile Input Integration Blueprint

## Current safe boundary

`MainActivity.nativeCricketGetRenderContext()` returns `-1` until a present, `1` for renderer menu-like/large-panel, and `0` for open 3D play. `nativeCricketInvalidateRenderContext()` clears validity at surface destruction, pause, and destroy. The Java/native boundary transports a classification only; it must not be interpreted as batting or bowling state.

## Future adapter contract

A future gameplay adapter should return an explicit enum plus validity/version and be backed by verified original engine symbols/fields. Unknown, stale, lifecycle-invalidated, or unrecognized values must hide the HUD. Reads must occur on a safe engine thread or use a synchronized/atomic snapshot. Do not expose raw guest pointers across JNI. Input must remain owned by the original menu whenever HUD context is unknown or menu-like.

## Not implemented

No exact action scan-code mappings were asserted. Names in prior reports (RUN, CANCEL, SWING, CUTTER, BOUNCER) are not established here as correct/context-independent. No running, bowling, or fielding controls were added. No original menu changes or mode persistence were added.

## State discovery needed

Resolve function address to translation implementation and callsites; identify live state machine/current state storage; decode role and ball phase values; trace original input consumption; validate transitions against actual gameplay on device. Only then map Android touch actions to original key states with balanced down/up, cancel, pointer ownership, and lifecycle reset semantics.
