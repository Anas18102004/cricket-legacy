# Codex Phase G Joystick Batting Test

**Overall status: PARTIAL**

- **PASS (unit scope):** The mocked test confirmed the default deadzone, scaled axis response, and zero axes on pointer release.
- **PASS (runtime initialization):** The app loaded the native library and rendered a match on the x86_64 emulator.
- **NOT TESTED:** Classic Joystick and Pro Joystick match behavior, crease movement, batting footwork, shot direction, simultaneous real touch/button input, and unintended batsman movement.
- **Source finding:** The native touch handler updates a gamepad axis for any pointer assigned to the joystick, independent of batting mode. No separate Pro crease-movement mapping exists in this code path. The Java joystick is visual feedback; native joystick geometry and Java geometry are independently calculated and differ in radius/scale, so visual hit regions may not align.

No claim is made that joystick input produced a successful in-match action.
