# Codex Phase G Performance Test

**Overall status: PARTIAL**

Device under test: x86_64 Android emulator `sdk_gphone64_x86_64`; this is not representative of an ARM64 phone.

- Native telemetry during the captured runtime reported approximately **16.9–43.8 FPS** in the short sampled window. Logs are in [emulator-runtime-log.txt](artifacts/phase-g-codex-validation/emulator-runtime-log.txt) and [emulator-touch-trace.txt](artifacts/phase-g-codex-validation/emulator-touch-trace.txt).
- `dumpsys meminfo` snapshots are in [emulator-meminfo.txt](artifacts/phase-g-codex-validation/emulator-meminfo.txt). Runtime log RSS increased from roughly 177 MiB to 266 MiB over the captured period. This short observation cannot establish a leak, but the increase needs investigation.
- **NOT TESTED:** Separate menu versus match benchmark, average/minimum frame time over a controlled duration, CPU/GPU utilization, input-to-visible-response latency, thermal behavior, ARM64 performance, or a comparable Phase F baseline.
- **Limitation:** The app's telemetry reads engine frames on the Java render callback and logs once per 60 callbacks; the available samples are short and variable. No reproducible performance comparison was produced.

No conclusion of “no significant regression” or 60 FPS is supported.
