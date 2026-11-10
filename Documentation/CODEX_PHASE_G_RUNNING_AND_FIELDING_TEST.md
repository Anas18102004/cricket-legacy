# Codex Phase G Running and Fielding Test

**Overall status: NOT TESTED (gameplay); automatic fielding preservation: PARTIAL by source inspection**

- **NOT TESTED:** RUN/CANCEL timing, repeated run behavior, ball-dead hiding, wickets, fielder movement, pickup, catches, wicketkeeper, boundaries, throws, and run-outs.
- **PASS (source scope only):** This Phase G change adds touch/input/HUD code; no fielder AI or catch implementation is present among the inspected changed files. This does not prove runtime fielding behavior.
- **Source finding:** RUN is bound to `DIK_D`; CUT is also `DIK_D`. CANCEL and standard delivery both use `DIK_S`. The phase variable is recorded but not consulted to disambiguate these controls.
- **Source finding:** `BTN_DIVE`, throw actions, and manual fielding mode are declared in `mobile_input.h` but no corresponding buttons are created in the Android HUD. Thus the reported DIVE and throw HUD behavior is not present in the inspected view.
- **Source reliability finding:** On release of any non-toggle native button, `mobile_input.c` releases every pressed non-toggle button, rather than only the button owned by that pointer. On `ACTION_CANCEL`, only the pointer ID passed to the handler is processed. Multi-pointer cancellation can leave other pointer states or buttons active.

No fielder action was triggered or credited as tested.
