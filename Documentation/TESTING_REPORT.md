# CRICKET LEGACY — Quality Assurance & Testing Report
**Document Status:** Baseline Test Suite Definition  
**Last Updated:** Phase 0 Inspection & Review  

---

## 1. Automated Test Architecture

The project utilizes Unity Test Framework (`com.unity.test-framework`) organized into two distinct test domains:
1. **EditMode Tests (`Tests/EditMode/`):** Pure C# mathematical and rules logic validation without instantiating scene graphics.
2. **PlayMode Tests (`Tests/PlayMode/`):** Dynamic runtime verification of physics trajectories, collision events, input gesture recognition, and match state transitions.

---

## 2. Test Suites Specification

### 2.1 Physics & Ball Flight Verification Suite (`PhysicsVerificationTests.cs`)
- `Test_BallDrop_RestitutionMatchesPitchCoefficient`: Verifies coefficient of restitution $e_n$ when dropping ball from 5m onto dry pitch.
- `Test_MagnusEffect_GeneratesLateralDrift`: Verifies that a spinning ball drifts perpendicular to velocity vector.
- `Test_DragCrisis_SlowsBallAccordingToReynoldsNumber`: Validates velocity decay curve over 20m delivery flight.
- `Test_SweptCollision_DetectsBatIntersectionAtHighVelocity`: Simulates $160\text{ km/h}$ delivery against moving bat collider; verifies zero tunneling occurrences across 500 test iterations.

### 2.2 Batting & Scoring Mechanics Suite (`BattingMechanicsTests.cs`)
- `Test_SweetSpotHit_ProducesMaxExitVelocity`: Verifies bat center of percussion produces $e_{\text{eff}} \approx 0.82$.
- `Test_EdgeContact_ProducesDeflectionAngle`: Verifies lateral edge contacts deflect between $35^\circ$ and $75^\circ$.
- `Test_TimingPenalty_ReducesExitSpeed`: Verifies early/late offsets cleanly reduce quality score $Q$.

### 2.3 Match Rules & State Machine Suite (`MatchRulesTests.cs`)
- `Test_T20_PowerplayEnforcesFielderLimits`: Validates that maximum 2 fielders can be positioned outside 30-yard ring in overs 1-6.
- `Test_NoBall_GrantsFreeHit`: Validates next delivery state marks free hit flag.
- `Test_RunOut_ValidatesCreaseDistance`: Verifies wicket broken while runner $0.1\text{ m}$ outside crease awards wicket; grounded bat inside crease returns not out.

---

## 3. Hardware Test Devices Matrix

| Device Model | Chipset | RAM | OS Version | Target Profile | Test Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PC Development Host** | Core i5-12500H / RTX 3050 Laptop | 16GB+ | Windows 11 (26300) | Editor Simulation & Physics Bench | Verified |
| **Physical Android Test Tier 1**| Snapdragon 8 Gen 2 / Gen 3 | 12GB | Android 14+ | High / Ultra 60 FPS Target | Pending M1 Build |
| **Physical Android Test Tier 2**| MediaTek Helio G99 / Dimensity 6080 | 4GB / 6GB | Android 12 / 13 | Budget / Mid 30-60 FPS Profile | Pending M1 Build |
| **Physical iOS Test Device** | Apple A15 / A16 (iPhone 13 / 14 / 15) | 4GB / 6GB | iOS 17+ | Metal 60 FPS Profile | Pending Export |
