# CRICKET LEGACY — Physics Specification & Mathematical Model

---

## 1. Flight Dynamics Equation of Motion

The movement of a cricket ball in flight is governed by four vector forces:

$$m \frac{d\vec{v}}{dt} = m\vec{g} + \vec{F}_{\text{drag}} + \vec{F}_{\text{Magnus}} + \vec{F}_{\text{seam}}$$

Where:
- $m$: Mass of cricket ball ($0.156\text{ kg} \text{ to } 0.163\text{ kg}$, calibrated to $0.160\text{ kg}$).
- $\vec{g}$: Acceleration due to gravity ($[0, -9.80665, 0]^T \text{ m/s}^2$).
- $\vec{v}$: Ball linear velocity vector relative to ambient air.
- $\vec{\omega}$: Ball angular velocity (spin) vector.

---

## 2. Aerodynamic Forces

### 2.1 Quadratic Drag Force
$$\vec{F}_{\text{drag}} = -\frac{1}{2} C_d(Re) \cdot \rho \cdot A \cdot |\vec{v}| \cdot \vec{v}$$
- $\rho$: Air density at sea level ($1.225\text{ kg/m}^3$).
- $A$: Cross-sectional area of ball ($\pi r^2$, radius $r = 0.036\text{ m} \implies A \approx 4.07 \times 10^{-3}\text{ m}^2$).
- $C_d$: Drag coefficient calibrated against Reynolds number ($Re = \frac{\rho v (2r)}{\mu}$):
  - Sub-critical laminar flow ($v < 30\text{ m/s}$): $C_d \approx 0.45$.
  - Critical drag crisis transition ($30 \le v \le 42\text{ m/s}$): $C_d$ drops to $0.28 - 0.35$.
  - Post-critical: $C_d \approx 0.32$.

### 2.2 Magnus Effect (Spin & Drift)
$$\vec{F}_{\text{Magnus}} = \frac{1}{2} C_L \cdot \rho \cdot A \cdot r \cdot (\vec{\omega} \times \vec{v})$$
- $C_L$: Lift coefficient parameterized by spin parameter $S_p = \frac{r |\vec{\omega}|}{|\vec{v}|}$.
- Generates:
  - Top-spin: Negative vertical lift, forcing the ball to dip rapidly onto the pitch.
  - Back-spin: Positive vertical lift, creating floating flight.
  - Drift (Off-spin / Leg-spin): Lateral force deviating the ball through the air before bouncing.

### 2.3 Seam Angle & Asymmetric Swing
When the seam is inclined at angle $\theta_{\text{seam}}$ ($10^\circ \text{ to } 20^\circ$) to the flight path:
$$\vec{F}_{\text{seam}} = C_{\text{swing}} \cdot \rho \cdot A \cdot |\vec{v}|^2 \cdot \hat{n}_{\text{lateral}}$$
- **Conventional Swing:** Boundary layer on the rough/seam side transitions to turbulent flow earlier than the smooth side, creating lateral pressure differential toward the seam direction.
- **Reverse Swing:** At high speeds ($> 38\text{ m/s}$) with an older ball ($BallAge > 40\text{ overs}$), both sides become turbulent; the rough side separates earlier, swinging opposite to seam inclination.

---

## 3. Pitch Collision & Bounce Model

At pitch contact time $t_c$, the impact point undergoes normal compression and tangential friction:

$$\vec{v}_{\text{post}} = \begin{bmatrix} (1 - \mu_k) v_{x,\text{pre}} \\ -e_n \cdot v_{y,\text{pre}} \\ (1 - \mu_k) v_{z,\text{pre}} \end{bmatrix}$$

### 3.1 Pitch Surface Parameters

| Pitch Type | Normal Restitution ($e_n$) | Tangential Friction ($\mu_k$) | Seam Deviation Variance ($\sigma_\theta$) | Spin Bite Multiplier |
| :--- | :--- | :--- | :--- | :--- |
| **Hard / Dry** | $0.62$ | $0.28$ | Low ($1.5^\circ$) | $1.0\times$ |
| **Green / Grass** | $0.58$ | $0.22$ | High ($4.5^\circ$) | $0.7\times$ |
| **Dusty / Subcontinent**| $0.52$ | $0.42$ | Moderate ($2.8^\circ$) | $1.8\times$ (High turn) |
| **Damp / Wet Outfield**| $0.45$ | $0.35$ | Low ($1.2^\circ$) | $0.6\times$ (Low skid) |

---

## 4. Bat-Ball Contact Mechanics

The bat blade is modeled as an oriented box collider containing dynamic elastic response zones.

### 4.1 Contact Impulse Equation
Upon collision with the bat moving at velocity $\vec{v}_{\text{bat}}$ with effective inertia $I_{\text{bat}}$:

$$\vec{J} = -(1 + e_{\text{eff}}) \frac{\vec{v}_{\text{rel}} \cdot \hat{n}}{\frac{1}{m_{\text{ball}}} + \frac{(\vec{r}_{\text{contact}} \times \hat{n})^2}{I_{\text{bat}}}} \hat{n}$$

- Effective restitution $e_{\text{eff}}$ varies across the bat surface:
  - Center of Percussion (Sweet Spot): $e_{\text{eff}} = 0.82$.
  - Upper Blade: $e_{\text{eff}} = 0.52$.
  - Bat Toe: $e_{\text{eff}} = 0.40$.
  - Outside Edges: Deflection impulse with normal rotated $\approx 45^\circ - 70^\circ$.

---

## 5. Numerical Substepping Integrator

To avoid tunneling at velocities up to $45\text{ m/s}$ ($162\text{ km/h}$), ball simulation updates at a fixed internal rate of $\Delta t = \frac{1}{120}\text{ s}$ ($8.33\text{ ms}$) using 4th-Order Runge-Kutta (RK4):

$$\vec{k}_1 = \vec{a}(t_n, \vec{x}_n, \vec{v}_n)$$
$$\vec{k}_2 = \vec{a}\left(t_n + \frac{\Delta t}{2}, \vec{x}_n + \frac{\Delta t}{2}\vec{v}_n, \vec{v}_n + \frac{\Delta t}{2}\vec{k}_1\right)$$
$$\vec{k}_3 = \vec{a}\left(t_n + \frac{\Delta t}{2}, \vec{x}_n + \frac{\Delta t}{2}\vec{v}_n + \frac{\Delta t^2}{4}\vec{k}_1, \vec{v}_n + \frac{\Delta t}{2}\vec{k}_2\right)$$
$$\vec{k}_4 = \vec{a}(t_n + \Delta t, \vec{x}_n + \Delta t \vec{v}_n + \frac{\Delta t^2}{2}\vec{k}_2, \vec{v}_n + \Delta t \vec{k}_3)$$
$$\vec{v}_{n+1} = \vec{v}_n + \frac{\Delta t}{6}(\vec{k}_1 + 2\vec{k}_2 + 2\vec{k}_3 + \vec{k}_4)$$
$$\vec{x}_{n+1} = \vec{x}_n + \Delta t \vec{v}_n + \frac{\Delta t^2}{6}(\vec{k}_1 + \vec{k}_2 + \vec{k}_3)$$

Continuous Swept Sphere checks verify if the ray between $\vec{x}_n$ and $\vec{x}_{n+1}$ intersects the moving bat volume during contact frames.
