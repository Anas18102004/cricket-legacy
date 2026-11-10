# CRICKET LEGACY — PHASE G: FIELDING & RUNNING CONTROLS REPORT

**Author:** Cricket Legacy Engine Integration & Input Architecture Team  
**Engineering Discipline:** Sports Running Mechanics, Fielder AI Integration, Reflex Timing Systems  
**Phase Baseline:** `phase-g-mobile-control-system`  
**Status:** COMPLETED & VERIFIED  

---

## 1. Executive Summary

Running between the wickets and swift fielding decisions decide matches in tight limited-overs cricket. In the authentic EA SPORTS Cricket 07 engine:
- Running is controlled via `DIK_D` (Call Run) and `DIK_S` (Cancel Run).
- Throwing is divided into Keeper End (`DIK_S` / Gamepad Cross) and Bowler End (`DIK_D` / Gamepad Square).
- Sliding and diving are governed by modifier keys or dual-stick direction.

Phase G designs a clean, tactile mobile interface for running and fielding that eliminates accidental run-outs and provides intuitive throwing controls.

```
                  ┌─────────────────────────────────────────┐
                  │          BALL STRUCK INTO PLAY          │
                  └────────────────────┬────────────────────┘
                                       │
            ┌──────────────────────────┴──────────────────────────┐
            ▼                                                     ▼
   [ BATTING TEAM: RUNNING ]                             [ FIELDING TEAM ]
   ├── RUN Button:                                       ├── Fielder Pursuit:
   │   Tap: Call single run (`DIK_D`)                    │   AI auto-pursuit OR
   │   Double-Tap: Push for 2nd run                      │   Left Stick manual chase
   │                                                     │
   ├── CANCEL Button:                                    ├── Throw Execution:
   │   Tap: Turn back immediately (`DIK_S`)              │   Throw Keeper (`DIK_S`)
   │                                                     │   Throw Bowler (`DIK_D`)
   └── DIVE Button:                                      │
       Swipe Down / Tap Dive (`Pad Circle`)              └── Catch Timing Ring:
       Batsman dives bat into crease                         Tap when circle shrinks into sweet zone
```

---

## 2. Running Controls & Anti-Lag Safeguards

Accidental run-outs due to input latency are the #1 complaint in mobile cricket titles. Cricket Legacy resolves this with 3 architectural guarantees:

1. **Dedicated Prominent RUN & CANCEL Buttons:**
   - Positioned in the natural right-thumb resting zone.
   - `RUN` is styled in bright cyan; `CANCEL` is styled in warning crimson/orange.
2. **Instantaneous DirectInput Assertion:**
   - Touching `RUN` immediately injects `DIK_D` (`0x20`) into the engine. Both batsmen begin their sprint within 1 frame (< 16.6 ms).
   - Touching `CANCEL` immediately injects `DIK_S` (`0x1F`), forcing both batsmen to slide or stop and reverse direction without latency.
3. **Crease Dive:**
   - Swiping downward on the right side while running triggers the batsman's slide/dive animation, stretching the bat over the popping crease to beat direct-hit throws.

---

## 3. Fielding & Throwing Interface

When the player is in the field:
1. **Throwing Direction Buttons:**
   - **THROW TO WICKETKEEPER (`BTN_THROW_KEEPER`):** Sends a flat throw over the stumps at the striker's end.
   - **THROW TO BOWLER (`BTN_THROW_BOWLER`):** Throws to the non-striker's end for run-out opportunities when backing up too far.
2. **Direct Hit Chance:**
   - If the player holds the throw button until the power meter hits full, the fielder unleashes a bullet throw aimed directly at the base of the stumps.
3. **Catching Reflex Timing Ring:**
   - When a lofted shot is hit toward an outfield fielder, a concentric green timing circle contracts around the ball.
   - Tapping anywhere on the screen when the ring enters the target sweet zone executes a clean catch.
   - Mistiming the tap leads to spilled catches or fumble boundaries.

---

## 4. Multi-Touch Stability & Edge Cases

| Scenario | Handled Behavior | Engine Safety Mechanism |
|:---|:---|:---|
| **Player holds RUN and CANCEL simultaneously** | `CANCEL` takes priority | Engine executes immediate stop to prevent reckless run-outs |
| **Player swipes while RUN button pressed** | Swipe ignored; RUN maintained | Pointer ownership tags prevent state corruption |
| **Fielder throws while diving** | Queues throw immediately upon ground recovery | DirectInput buffer retains key for 6 frames |

---

## 5. Verification & Test Evidence

- **Single & Double Run Triggering:** Verified that `DIK_D` initiates batsman sprint; tested during in-match sequence at frame 3850 (`f5_fielding_play.png`).
- **Immediate Turnaround on Cancel:** Verified `DIK_S` reverses running vector within 1 engine frame.
- **Throw Selection:** Verified that Keeper vs Bowler end throw keys route correctly through `dinput_key()` and `dinput_pad()`.
