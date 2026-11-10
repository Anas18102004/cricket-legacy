# State Evidence Matrix

Categories: **A** verified engine state; **B** verified function behavior; **C** renderer-only; **D** observed correlation; **E** unsupported hypothesis.

| Target | Evidence/category | Manual? | Before/after values | Confidence |
|---|---|---:|---|---|
| Menu-like vs open 3D render context | C: renderer present classification | No (prior samples scripted) | Earlier G.2 renderer samples only | Medium for coarse graphics classification |
| Main menu / team / settings / toss / loading individually | No distinguishing signal | No | None | Unknown |
| User batting / AI batting | RTTI candidate names only | No | None | Unknown |
| Bowling preparation/run-up/release | RTTI candidate names only; renderer is not phase signal | No | None | Unknown |
| Ball flight/bounce/hit/dead | CCrBall/end-state names only | No | None | Unknown |
| Running/cancel/run completion | No verified state or action acceptance | No | None | Unknown |
| Automatic fielding/catch/pickup | RTTI candidate names; no live execution observation | No | None | Unknown |
| Wicket/over/innings/match end | State/script names are leads only | No | None | Unknown |
| Host key down/up delivery | B: source path in `host/input.c` and `host/dinput.c` | No gameplay validation | Source code inspected | High for host-side representation; none for game action meaning |
| `0x008A1000` game state candidate | E: unsupported and unused | N/A | None | Reject; do not use |

No G.3 gameplay state is category A. All manual scenario values remain NOT TESTED.
