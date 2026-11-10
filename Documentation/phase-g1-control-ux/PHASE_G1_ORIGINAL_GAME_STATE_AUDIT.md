# Phase G.1 Original Game State Audit

**Finding: BLOCKED — no reliable match/menu phase detector found.**

- `cricket07.h` exposes the profile's menu cursor address (`0xa01970`) but no scene or match-state address.
- Host `input.c`/`dinput.c` translate key and device state; they do not expose Cricket 07 match phases.
- `MainActivity` declares `nativeCricketSetGamePhase`, but no caller sets it from observed game state.
- `mobile_input.c` records a phase supplied by the host; it does not derive one.
- `main.c` contains `s_state_tick` and frame-number-triggered key presses/captures that automate splash, menus, toss and match progression. This is a script, not state observation, and cannot distinguish real gameplay events. It is explicitly unsuitable for HUD visibility.
- The available translated C files are address-based recompiled functions, with no verified semantic state mapping found in this audit.

Consequently menu, batting-ready, ball-in-play, running, ball-dead, wicket, bowling and pause states cannot be reliably distinguished from current source interfaces. No timer or guessed memory predicate was added. The runtime fails closed and hides gameplay controls. A verified source-state mapping is a prerequisite for enabling the contextual HUD.
