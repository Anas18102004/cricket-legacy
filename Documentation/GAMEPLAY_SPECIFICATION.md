# CRICKET LEGACY — Detailed Gameplay Specification

---

## 1. Batting Mechanics Specification

### 1.1 Spatial Coordinate Frame & Footwork
- **Batting Stance Origin:** Batsman standing at batting crease ($z = 0\text{ m}$ along pitch axis, $x = 0\text{ m}$ aligned with middle stump).
- **Footwork Trigger States:**
  - **Front Foot ($Z \ge +0.4\text{ m}$):** Triggered automatically or via forward analog stick for full/good length deliveries. Maximizes sweet-spot area for straight drives, cover drives, and on-drives.
  - **Back Foot ($Z \le -0.3\text{ m}$):** Triggered automatically or via backward analog stick for short-of-a-length, short balls, and bouncers. Essential for cut shots, hooks, and pull shots.
  - **Misjudged Footwork Penalty:** Playing forward to a short ball or back to a full ball reduces effective sweet spot by $60\%$ and dramatically increases edge / top-edge probability.

### 1.2 Bat Collision Zones & Coefficient of Restitution ($e$)

```
          [ HANDLE ]
+----------------------------+
|        Upper Blade         |  e = 0.50 (Dampened velocity)
|----------------------------|
|                            |
|       THE SWEET SPOT       |  e = 0.82 (Maximum exit velocity)
|                            |
|----------------------------|
|         Toe Region         |  e = 0.42 (High vibration, low launch)
+----------------------------+
  Left Edge / Right Edge: e = 0.35, Deflection Angle = 35° - 75°
```

### 1.3 Timing Windows (Normalized to Delivery Speed)
For a standard fast delivery ($140\text{ km/h} \approx 38.89\text{ m/s}$):
- **Perfect Timing Window ($[-15\text{ ms}, +15\text{ ms}]$):** Maximum exit velocity multiplier ($1.0\times$ to $1.2\times$), precise placement vector.
- **Good Timing Window ($[-35\text{ ms}, -15\text{ ms}]$ or $[+15\text{ ms}, +35\text{ ms}]$):** $0.85\times$ exit velocity, slight deviation in direction ($\pm 5^\circ$).
- **Early Timing ($[-80\text{ ms}, -35\text{ ms}]$):** Leading edge probability surges to $45\%$; ball elevated into infield catchers.
- **Late Timing ($[+35\text{ ms}, +80\text{ ms}]$):** Inside/outside edge probability surges to $60\%$; ball deflects toward slips or stumps.
- **Miss ($<-80\text{ ms}$ or $>+80\text{ ms}$):** Complete air swing; ball passes through to wicketkeeper or hits stumps/pads.

---

## 2. Bowling Mechanics Specification

### 2.1 Pitch Coordinate System
Pitch dimensions: Standard 22 yards ($20.12\text{ m}$) length $\times$ 10 feet ($3.05\text{ m}$) width.
- Length Zones (measured from batting stumps):
  - **Yorker:** $0.0\text{ m}$ to $1.8\text{ m}$
  - **Full Pitch:** $1.8\text{ m}$ to $4.5\text{ m}$
  - **Good Length:** $4.5\text{ m}$ to $7.0\text{ m}$
  - **Short of a Length:** $7.0\text{ m}$ to $9.5\text{ m}$
  - **Short / Bouncer:** $> 9.5\text{ m}$
- Lateral Line Zones (relative to middle stump $x = 0$):
  - **Down Leg:** $x < -0.35\text{ m}$ (Right-handed batter)
  - **Stumps Line:** $-0.25\text{ m} \le x \le +0.25\text{ m}$ (Leg, Middle, Off)
  - **Corridor of Uncertainty (4th/5th stump):** $+0.25\text{ m} < x \le +0.65\text{ m}$
  - **Wide Outside Off:** $x > +0.90\text{ m}$

### 2.2 Bowling Variation Physics Modulation

| Variation | Speed Delta | Magnus Vector Adjustment | Pitch Bounce Factor |
| :--- | :--- | :--- | :--- |
| **Stock Outswinger** | $0\text{ km/h}$ | Lateral drift to off-side ($+0.25\text{ m}$) | Standard ($0.58$) |
| **Inswinger** | $0\text{ km/h}$ | Lateral drift to leg-side ($-0.25\text{ m}$) | Standard ($0.58$) |
| **Reverse Swing** | $-3\text{ km/h}$ | Exponential late curve into batter | Low bounce ($0.52$) |
| **Off-Cutter / Slower Ball** | $-18\text{ km/h}$ | Negative lift, deviation off pitch seam | Dampened ($0.50$) |
| **Bouncer** | $+2\text{ km/h}$ | Steep upward exit angle ($+15^\circ$) | High elasticity ($0.65$) |
| **Off-Break (Spin)** | Baseline | Sharp bite toward leg side post-bounce | Grip angle $+8^\circ$ |
| **Googly (Wrist Spin)** | Baseline | Breaks opposite to leg-break direction | Grip angle $+10^\circ$ |

---

## 3. Running Between Wickets Specification

- **Runner States:** `IDLE_IN_CREASE`, `ACCELERATING`, `SPRINTING`, `TURNING`, `DIVING`, `CROSSING_LINE`.
- **Sprint Physics:**
  - Top speed: $7.5\text{ m/s}$ (fast runner) to $6.2\text{ m/s}$ (slower runner).
  - Acceleration: $3.2\text{ m/s}^2$.
  - Turnaround delay: $0.4\text{ s}$ deceleration + $0.25\text{ s}$ pivot time.
- **Run Out Geometry:**
  - Wicket is broken if ball touches stump collider while neither runner bat nor foot is grounded behind the popping crease line ($1.22\text{ m}$ in front of stumps).
  - Sliding bat mechanic: Bat tip grounded behind line safely confirms crease entry even if body is in the air.

---

## 4. Artificial Intelligence (AI) Specification

### 4.1 Batting AI Decision Tree
- Evaluates incoming delivery line, length, pace, and bowler hand.
- Compares against Match Situation:
  - If Required Run Rate (RRR) $> 9.0$: Shifts priority to lofted drives, pulls, and sweeps ($75\%$ aggression).
  - If RRR $< 4.0$ or Wickets $< 3$: Prioritizes defensive preservation, rotated singles ($25\%$ aggression).
  - Evaluates fielder density in target sectors before committing to aerial shots.

### 4.2 Bowling AI Strategy
- Tracks player wagon wheel and historical shot preferences in the current match.
- If player repeatedly scores on the off-side: Sets defensive off-side field and bowls tight stump-to-leg line.
- Uses delivery sequences: E.g., two short balls followed by an inswinging yorker at high speed.
