# Instrumentation Performance Report

## Instrumentation prepared

CMake option `CRICKET_G3_MANUAL_TRACE` (default OFF) creates an operator-driven diagnostic build. It disables the Phase F frame-count progression script and enables a 128-entry fixed-size touch-event ring buffer. Each record contains monotonic host callback tick, Android action, pointer id, screen x/y, and renderer context (`-1` unknown, `0` open 3D, `1` menu-like). The UI/render callback drains events to logcat. Overflow is counted and reported. There are no per-touch heap allocations, guest-memory reads, or gameplay writes.

## Measurements

**NOT MEASURED.** No manual diagnostic session was performed. No before/after frame-time, memory, logging-volume-under-play, or behavioral equivalence measurements are claimed. Static buffer payload is 128 × 28 bytes (3,584 bytes, excluding alignment); actual compiler layout and runtime overhead have not been measured.

## Limits

This trace is an operator-input timeline, not original function or variable tracing. It is intended to annotate later manually observed events and validate that the frame-count script is absent. Do not treat it as state detection. The default build leaves the option disabled.
