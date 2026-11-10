# Phase G.1 Performance Report

## Build and runtime scope

- Native libraries rebuilt for x86_64 and arm64-v8a; x86_64 app installed and launched.
- The change removes the permanently invalidating overlay from the screen in the current fail-closed path. This should reduce Java drawing work, but no controlled before/after benchmark was run.
- No menu-versus-match FPS, frame-time, CPU/GPU, input-latency or memory comparison was collected under matched conditions.
- ARM64 compilation is PASS; ARM64 runtime/performance is NOT TESTED. Physical device is NOT TESTED.

**Conclusion:** Performance impact is not quantified. No FPS improvement claim is made.
