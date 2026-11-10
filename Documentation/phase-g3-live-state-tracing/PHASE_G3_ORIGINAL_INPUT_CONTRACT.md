# Original Input Contract

## Verified host-side path

`host/input.c` `input_key_dik` routes DIK events through `key_event_dik`. For a mapped key, the host calls `dinput_key`, updates `g_keys[vk]`, and posts key-down `0x100` or key-up `0x101` message with scan-code data. `host/dinput.c` `dinput_key` updates `g_dik[256]` and adds a buffered event. `dev_GetDeviceState` exposes keyboard state to the emulated DirectInput consumer.

This proves how host input is represented and delivered, not which DIK actions Cricket 07 accepts during batting/bowling/running. No game action mapping is asserted.

## Manual input evidence

No manual gameplay inputs were recorded. G.3 diagnostic mode queues touch action, pointer id, screen coordinates, host frame tick and renderer classification. It does not convert a touch into new gameplay key actions; existing G.1 fail-closed behavior remains.

## Unknowns

Accepted batting/bowling/run inputs, phase dependence, simultaneous key handling, whether down/up pairs are required by specific actions, and game-side rejection conditions are all UNKNOWN. Validate each with operator input and original execution evidence before assigning mappings.
