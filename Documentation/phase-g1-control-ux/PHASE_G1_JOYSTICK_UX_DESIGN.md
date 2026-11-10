# Phase G.1 Joystick UX Design

**Runtime status: NOT ENABLED.** The Phase G joystick was too large, independently laid out in Java and C, and anchored to full view dimensions rather than a verified game viewport.

The correction hides it in all current runtime scenes. A polished joystick design is deferred until the game viewport transform and batting state can be observed reliably. The eventual design should derive its safe bounds from the rendered surface content rectangle, apply density-independent sizing and insets, mirror placement for left-handed users, and use one shared geometry model for drawing and hit testing. No mock is presented as runtime evidence.
