# Engine Function Trace

## Source inventory inspected

- `D:/cricket07-browser-src/runtime/runtime.c`: generated function dispatch and optional indirect-call diagnostic (`RC_TRACE_CALL`). That facility traces matching indirect calls only; translated functions often call each other directly, so it is not a complete call graph.
- `runtime/runtime.h`: guest image/state bounds and optional instruction trace globals. `g_trace` is a linear instruction buffer used by test harnesses, not a bounded live ring; enabling full instruction capture on Android would have high overhead and large volume.
- `out_all/recomp_034.c`: `f_0055c2f0` is labeled `CBatsman::vfunc_0` and immediately adjusts ECX then tail-jumps to `f_0055c9f0`. This only establishes a wrapper/delegation, not batting-state semantics.
- `out_all/recomp_046.c`: `f_005aabe0` labeled `CFielderManager::vfunc_0` similarly adjusts ECX and jumps into another function. Name does not establish fielder AI behavior.
- `out_all/recomp_047.c`: `f_005bc290` labeled `CCatchBehaviour::vfunc_2`; body must be followed through data and callers before behavior can be named.
- `host/input.c`: `input_key_dik` maps DIK to a virtual-key entry, updates the DirectInput key state, then posts a `WM_KEYDOWN` (`0x100`) or `WM_KEYUP` (`0x101`) style message.
- `host/dinput.c`: `dinput_key` updates the keyboard byte array and buffered event queue. `dev_GetDeviceState` supplies keyboard state to guest polling.

## Trace limitations

The Android project compiles generated translation units from the separate `D:/cricket07-browser-src/out_all` tree. That tree is outside the writable project root. No generated engine function was patched. The G.3 diagnostic mode therefore captures touch event time/order and renderer context but not original function entries, guest memory, or action acceptance.

## Candidate functions

RTTI/function inventory candidates (not verified gameplay contracts): CricketGame/GameFaceState/FrontendState/LoadingScreenState; CBatsman/CBatsmanBase; CBowler; CCrBall; CFielderManager; CCatchBehaviour; CKeeperBehaviour; run-out checker and end-ball/end-over/end-innings script classes. Address/name matches are search leads only.

Next safe trace step: instrument a small set of recovered function entry points through a diagnostic-only, versioned hook after deriving real callers/callees and argument contracts. Do not blanket-trace instructions.
