# Ball and Running State Analysis

**Ball lifecycle and running state: UNKNOWN.** No verified decoded ball-state enum or original running/cancel acceptance transition was identified.

The translated metadata includes `CCrBall`, `CSCEndBall`, `CSCEndOver`, `CSCEndInnings`, and `CSCEndMatch` names. Names do not establish runtime meaning, object layout, or invocation timing. No safe evidence currently answers when the game accepts run, when a runner is already committed, when cancel is valid, how a run-out changes state, or precisely when the ball becomes dead.

`RUN`/`CANCEL` must remain hidden. Do not use a fixed timer. Required next investigation: decode ball and innings update functions, trace input action acceptance, instrument state transitions read-only, and compare normal run, second run, cancellation, wicket, boundary, and dead-ball outcomes in actual gameplay.
