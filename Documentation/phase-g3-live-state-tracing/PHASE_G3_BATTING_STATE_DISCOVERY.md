# Batting State Discovery

**Status: NOT TESTED; original batting state UNKNOWN.** No manual delivery/shot session was recorded.

Static evidence includes RTTI/function labels for `CBatsman`, `CBatsmanBase`, and generated functions such as `f_0055c2f0`. Inspection of the latter shows a small wrapper/tail jump, not a decoded state check. No verified live batsman pointer, batting-side role field, striker identity, delivery window, or shot-acceptance flag is available.

The opt-in G.3 manual trace records Android touch event timestamp/order and coarse render context only. It cannot prove the engine accepted a shot. Required follow-up: manually label deliveries and shots, trace decoded candidate function paths and safe fields, and reproduce across multiple balls. No batting controls were enabled.
