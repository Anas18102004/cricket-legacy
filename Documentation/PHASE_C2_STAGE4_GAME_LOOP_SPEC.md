# CRICKET LEGACY — Phase C.2 Stage 4: Engine Main Operating Loop Specification
**Document ID:** `CL-C2-STAGE4-SPEC-v1.0`  
**Classification:** System Architecture & Win32 Event Pump Integration  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** APPROVED & EMPIRICALLY VERIFIED  

---

## 1. Executive Overview

In 32-bit Windows games (such as EA SPORTS Cricket 07, 2006), the game engine drives its entire runtime lifecycle through the **Win32 Message Pump Operating Loop**.

The operating loop performs four critical duties on every tick:
1. **Message Dispatching:** Queries pending Windows OS messages via `PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)`, translating and dispatching them to the registered window procedure (`lpfnWndProc`).
2. **Input Processing:** Handles mouse movements (`WM_MOUSEMOVE`), button presses (`WM_LBUTTONDOWN`/`UP`), and keyboard events (`WM_KEYDOWN`/`UP`).
3. **Application Lifecycle:** Responds to window activation (`WM_ACTIVATEAPP`), minimize/restore, and clean termination (`WM_QUIT`).
4. **Frame Pacing & VSync:** Paces the engine to target refresh rates (typically 60 Hz = ~16.6 ms per tick).

Phase C.2 Stage 4 implements this complete message pump, input bridge, lifecycle state machine, and frame pacing on native Android.

---

## 2. Operating Loop Architecture

```
                    ┌──────────────────────────────────────────────────┐
                    │               Android Touch / OS Event           │
                    │         (MainActivity: onTouch, onPause)         │
                    └─────────────────────────┬────────────────────────┘
                                              │
                     game_loop_post_touch() / game_loop_set_paused()
                                              │
                                              ▼
                    ┌──────────────────────────────────────────────────┐
                    │            Win32 Message Queue (FIFO)            │
                    │        (src/game_loop.c: g_queue[256])           │
                    ├──────────────────────────────────────────────────┤
                    │ • Thread-safe mutex serialization                │
                    │ • Non-destructive peeking (PM_NOREMOVE)          │
                    │ • Monotonic timestamps (timer_time_get_time)     │
                    └─────────────────────────┬────────────────────────┘
                                              │
                        w_PeekMessageA() / w_GetMessageA()
                                              │
                                              ▼
                    ┌──────────────────────────────────────────────────┐
                    │           Engine Main Operating Loop             │
                    │          (Dedicated 64MB Game Thread)            │
                    ├──────────────────────────────────────────────────┤
                    │ while (!bQuit) {                                 │
                    │     while (PeekMessage(&msg, PM_REMOVE)) {       │
                    │         if (msg == WM_QUIT) break;               │
                    │         DispatchMessage(&msg);                   │
                    │     }                                            │
                    │     // Game frame tick                           │
                    │     game_loop_pace_frame(&last_ns, 60);          │
                    │ }                                                │
                    └─────────────────────────┬────────────────────────┘
                                              │
                                     w_DispatchMessageA()
                                              │
                                              ▼
                    ┌──────────────────────────────────────────────────┐
                    │            Registered Window Procedure           │
                    │          (guest_stdcall -> Test / Game)          │
                    └──────────────────────────────────────────────────┘
```

---

## 3. Subsystem Specifications

### 3.1 Message Queue Implementation
- **Queue Capacity:** 256 messages in a circular ring buffer (`g_queue[256]`).
- **Standard Messages:** Supports `WM_NULL`, `WM_PAINT`, `WM_SIZE`, `WM_ACTIVATEAPP`, `WM_KEYDOWN`, `WM_KEYUP`, `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, and `WM_QUIT`.
- **Non-Destructive Peeking:** Complies with `PM_NOREMOVE` (inspect without popping) and `PM_REMOVE` (pop from queue).

### 3.2 Android Input Translation
- **Touch Down:** Maps Android `ACTION_DOWN` at coordinates $(X, Y)$ to `WM_LBUTTONDOWN` (`0x0201`) with packed 32-bit coordinates `((uint32_t)Y << 16) | (uint32_t)X`.
- **Touch Move:** Maps `ACTION_MOVE` to `WM_MOUSEMOVE` (`0x0200`).
- **Touch Up:** Maps `ACTION_UP` to `WM_LBUTTONUP` (`0x0202`).
- **Key Events:** Maps Android key events to `WM_KEYDOWN` (`0x0100`) and `WM_KEYUP` (`0x0101`).

### 3.3 Android Lifecycle State Machine
- When Android triggers `onPause()` / backgrounding:
  - `game_loop_set_paused(true)` marks the engine paused.
  - Automatically posts `WM_ACTIVATEAPP` (`0x001C`) with `wParam = 0` (inactive).
- When Android triggers `onResume()` / foregrounding:
  - `game_loop_set_paused(false)` unpauses the engine.
  - Automatically posts `WM_ACTIVATEAPP` with `wParam = 1` (active).

### 3.4 60 Hz Frame Pacing (`game_loop_pace_frame`)
- Calculates ideal frame interval: $\Delta t_{\text{target}} = \frac{1,000,000,000}{60} = 16,666,666\text{ ns}$.
- Samples POSIX `CLOCK_MONOTONIC`.
- If the current frame tick completed faster than 16.66 ms, sleeps the remaining slice via `nanosleep()` to maintain consistent 60 FPS pacing without burning mobile battery.

---

## 4. Synthetic Test Program Specification

To adhere strictly to the project rule that **no claim of running authentic game code is made before the real files arrive**, Stage 4 is validated using:
- **Identifier:** `test_game_loop_program` ([`src/game_loop.c:266`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L266))
- **Validation Scope:**
  1. Registers synthetic window procedure `test_custom_wndproc`.
  2. Executes a multi-frame game loop pumping messages via `PeekMessageA`.
  3. Dispatches touch, keyboard, and custom user messages to `test_custom_wndproc`.
  4. Exercises lifecycle pause and resume state transitions.
  5. Measures and enforces 60 Hz frame pacing.
  6. Terminates on `WM_QUIT` and cleanly returns exit code `0xCAFE0040`.

---

## 5. Architectural Compliance Matrix

| Subsystem | Android C Implementation | Upstream Reference | Empirical Status |
| :--- | :--- | :--- | :---: |
| **Message Queue** | [`game_loop.c:48`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L48) | `host/user32.c:37` | **VERIFIED** |
| **PeekMessageA** | [`game_loop.c:160`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L160) | `host/user32.c:63` | **VERIFIED** |
| **DispatchMessageA** | [`game_loop.c:182`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L182) | `host/user32.c:82` | **VERIFIED** |
| **Window Proc Dispatch** | [`game_loop.c:85`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L85) | `host/user32.c:16` | **VERIFIED** |
| **Touch / Input Bridge** | [`game_loop.c:127`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L127) | `host/input.c:235` | **VERIFIED** |
| **Lifecycle Pause/Resume** | [`game_loop.c:116`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L116) | `host/window.c:18` | **VERIFIED** |
| **60 Hz Frame Pacing** | [`game_loop.c:141`](file:///d:/cricket07%20mobile%20app/android-native-prototype/src/game_loop.c#L141) | `host/core.c:514` | **VERIFIED** |
