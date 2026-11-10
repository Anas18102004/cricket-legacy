# Phase G.1 Input Mapping Audit

The upstream host key map verifies scan-code identity and dispatch mechanics, not Cricket 07 gameplay meaning. `input_key_dik` finds a scan-code entry and updates DirectInput, virtual-key state and a Win32 key message; `dinput_key` stores the state and queues the event.

| Binding in Phase G | DIK identity | Verified game action? |
|---|---|---|
| RUN = D | `0x20` maps to letter D | No. No authoritative Cricket 07 action binding was found. |
| CANCEL = S | `0x1F` maps to letter S | No. |
| SWING = A | `0x1E` maps to letter A | No. |
| CUT = D | `0x20` maps to letter D; duplicates RUN | No; no phase routing exists. |
| BOUNCER = W | `0x11` maps to letter W | No. |
| Stroke = Enter + direction arrows | Scan codes are valid arrows/Enter | The intended shot semantics were not established. |

Phase G has duplicate D and S actions with no proven phase gate. G.1 does not invent replacement keys. It disables touch-driven mobile control while phase is MENU/unknown and releases mobile state when an explicit non-batting phase is set. These mappings remain unapproved until verified against original Cricket 07 configuration/gameplay.
