# Phase G.2 Engine State Discovery

## Executive finding

**Decision: PARTIALLY PROVEN.** The current translated engine does not expose a verified match-phase API to the Android HUD. The renderer offers one read-only, coarse classification: menu-like/large 2D panel versus open 3D play. It is not suitable for deciding batting, bowling, ball-live, running, wicket, or fielding controls. The G.1 fail-closed behavior remains in place; no gameplay HUD was enabled.

## Evidence and limits

- `android-native-prototype/src/upstream_d3d8/render_rec.c:705-719`: `rec_present_impl` classifies a presented frame as menu-like when there is no recorded 3D scene, or when a recorded 2D panel exceeds 40% target width and 30% target height.
- `render_rec.c:693-712`: `rc_in_menu` and the new `rc_get_menu_context` expose that renderer classification. The adapter returns unknown until a frame has been presented.
- `main.c:330-335`: JNI returns `-1` unknown, `1` menu-like, `0` open play. `MainActivity.java:58-59` declares the reader and invalidator. Surface destruction, pause, and destroy invalidate the sample.
- `render_rec.c:695-712`: atomic builtins publish/read value and validity across renderer/UI threads. This signal is renderer context, not original gameplay state.
- `android-native-prototype/src/game_static_data.h:37` has `CRICKET_VAR_GAME_STATE 0x008A1000` with purported values Init/Menu/Match. No supporting symbol/profile/translation evidence was found; it is unreferenced and must not be treated as valid.
- `D:/cricket07-browser-src/profiles/cricket07.h:18` documents `PROFILE_CURSOR=0xa01970` (640x480 cursor floats), not a match phase.
- Ghidra metadata includes state machine and game class RTTI, including `Hbs::Cricket::CricketGame`, `GameFaceState`, `FrontendState`, `LoadingScreenState`, and `BackendState`. RTTI proves type names only, not live instance addresses, field offsets, enum values, or transition semantics.

## Status matrix

| Requested state | Finding | Confidence |
|---|---|---|
| Main menu, team selection, match settings, toss, loading | Can be grouped as menu-like/panel by renderer in sampled progression; cannot distinguish individually | Medium for coarse group only |
| Open play | Renderer reports open 3D context | Medium for rendering classification only |
| User batting / AI bowling | Unknown | None |
| Bowling preparation / run-up / release | Unknown | None |
| Ball released / in play / dead | Unknown | None |
| Running / cancellation / run completion | Unknown | None |
| User bowling / AI batting | Unknown | None |
| Automatic fielding / catch / pickup | Types and functions exist; live phase unverified | Low |
| Wicket / over / innings / pause / match complete | Unknown as gameplay states; pause-like panel may fall under coarse renderer class | None for exact state |

## Safety consequence

The renderer flag may be used to fail closed on menu-like frames in future, but must not be used to show batting/bowling/running controls. Unknown or invalid context must continue hiding gameplay controls. Future precise detection needs symbols/offsets recovered and validated against real interactive match transitions.
