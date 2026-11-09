# CRICKET LEGACY — Master Development Roadmap

```
Phase 1: Foundation (M1-M3)     -> Engine setup, 3D stadium scene, Ball physics suite
Phase 2: Core Gameplay (M4-M6)   -> Touch batting, Strategic bowling, Fielding & running
Phase 3: Match Systems (M7-M9)   -> State machine, rules/scoring, formats & tournaments
Phase 4: Polish & QA (M10)       -> Mobile optimization, device QA, release packaging
```

---

## Milestone Breakdown & Acceptance Criteria

### Milestone 1: Project Foundation & Architecture Setup
- **Objectives:** Establish Unity 6 LTS project, configure URP, setup project folder layout, New Input System, Git repository, and CI/CD/build scripts.
- **Deliverables:** Verified clean compiling Unity project with URP mobile pipeline and zero build errors.
- **Acceptance Criteria:** Target compilation verified; Android target API 34 configured; tests pass cleanly.

### Milestone 2: 3D Cricket Ground & Interactive Scene (Prototype 0.1 Stage 1)
- **Objectives:** Assemble the first 3D ground: 22-yard pitch, 3D stumps/bails, boundary rope, outfield, placeholder batter, bowler, and ball game objects, broadcast camera rig.
- **Deliverables:** Playable 3D stadium scene with functional orbit/broadcast camera switching.
- **Acceptance Criteria:** Real interactive 3D scene (no 2D screenshot fakes); camera aligns with standard cricket broadcast angles.

### Milestone 3: Ball Physics Simulation Engine & Verification Suite
- **Objectives:** Implement `BallPhysicsController` with 120Hz substepping, RK4 trajectory integration, aerodynamic drag, Magnus effect drift, and pitch bounce restitution.
- **Deliverables:** Interactive physics test scene with visual trajectory tracers, velocity graphs, and automated unit tests.
- **Acceptance Criteria:** Spin produces visible drift; bouncer elevates over stump height; pitch restitution matches theoretical values within $5\%$ error.

### Milestone 4: Batting Engine & Touch Gesture Controls
- **Objectives:** Implement `BatterController`, `SwipeDetector`, and `BatImpactCalculator`. Support 8 directional swipe sectors, sweet spot contact, edge deflections, and timing windows (Early, Perfect, Late).
- **Deliverables:** Playable batting scenario facing automated bowling deliveries.
- **Acceptance Criteria:** Timing directly determines exit speed and edge probability; 6+ distinguishable shot types play distinct animations and hit directions.

### Milestone 5: Bowling Engine & Delivery Systems
- **Objectives:** Implement `BowlerController`, delivery type selector (Pace, Swing, Off-Spin, Leg-Spin), pitch targeting marker, and release timing gauge.
- **Deliverables:** Integrated Batter vs Bowler duel mode.
- **Acceptance Criteria:** Player can accurately target line and length; bowler attributes constrain scatter; release timing directly impacts swing/pace.

### Milestone 6: Fielding, Wicketkeeping & Running
- **Objectives:** Implement fielder AI intercept pathfinding, ground ball collection, throwing mechanics to keeper/bowler, running between wickets, crease sliding, and run-out detection.
- **Deliverables:** Full fielding team on the ground responding dynamically to batted balls.
- **Acceptance Criteria:** Fielders do not teleport; run-outs occur when stumps broken before bat crosses crease; direct-hit chances governed by attributes.

### Milestone 7: Match State Machine, Rules & Scoring Engine
- **Objectives:** Complete `MatchStateMachine` (Toss, Over start, Ball in flight, Dead ball, Over change, Innings change, Result), scorecard tracking (runs, wickets, extras, wagon wheels), and broadcast HUD.
- **Deliverables:** Fully playable 5-over exhibition cricket match.
- **Acceptance Criteria:** Complete match can be played from toss to victory screen without crashes; umpire signals boundaries, wickets, and extras correctly.

### Milestone 8: Visuals, Stadiums & Audio Polish
- **Objectives:** Upgrade stadium environment (crowd instancing, floodlights, boundary cushions), player models, cricket equipment, sound effects (bat-crack, crowd applause, appeal sounds), and commentary system.
- **Deliverables:** Production-quality visual and audio presentation.
- **Acceptance Criteria:** Dynamic lighting transitions smoothly for Day/Night matches; audio triggers synchronize tightly with physics collisions.

### Milestone 9: Game Formats & Local Tournaments
- **Objectives:** Implement configurable rules engine supporting T20, ODI, and Multi-day Test match rules (follow-on, declaration, powerplays), plus local knock-out tournament progression.
- **Deliverables:** Multi-format game mode selector and tournament bracket system.
- **Acceptance Criteria:** Format-specific rules execute faithfully; tournament progress saves and restores seamlessly across sessions.

### Milestone 10: Mobile Optimization, Packaging & QA Release Candidate
- **Objectives:** Memory profiling, battery/thermal optimization, GPU frame time stabilization, Android APK/AAB build, and iOS Xcode project export.
- **Deliverables:** Release-ready Android APK/AAB package and signed iOS build candidate.
- **Acceptance Criteria:** Stable 60 FPS on mid-tier test hardware; resident RAM $< 850\text{ MB}$; zero memory leaks; compliance with Play Store requirements.
