# Ball and Running State Discovery

**Status: NOT TESTED; ball lifecycle and running state UNKNOWN.** No manual ball release, flight, bounce, shot collision, run, cancellation, pickup, dead-ball, or run-out transition was captured.

Types/classes such as `CCrBall`, run-out checker, and end-ball/end-over controllers are search leads. No live object pointer, field offset, enum, accepted run command, or dead-ball transition has been verified. Do not map controls from those names or from elapsed time.

The diagnostic build provides ordered touch events plus coarse renderer context. It does not inspect guest memory. For a later operator session, annotate each event with visible phase and replay/ball outcome; then use targeted function-level instrumentation only after contracts and callers are established.
