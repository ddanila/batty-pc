#!/usr/bin/env python3
"""One place decides where a stuck ball sits (known-bugs.md #12).

A ball held by the MAGNET/CATCH bonus rests at `ball.stuck_offset_x` —
the quantised offset the catch recorded (`& 0xFC`, capped `0x18`, so
0, 4, 8, 12, 16, 20 or 24). `rest_ball_on_bat()` is the one function
that knows this, including the 1 px drop for a MAGNET-held ball.

`redraw_frame`'s bat-only branch used to place the ball at the constant
BALL_X_OFFSET_ON_BAT instead, which agrees only when the catch happened
to land on 16. The displayed position could then disagree with the
launch direction, which is derived from the same offset
(`launch_offset = ball.stuck_offset_x - 4`).

No pixel gate catches that: it needs a CATCH at a non-default offset AND
a frame where the bat moved without a physics tick. `test-magnet-ball`
catches at an offset that happens to agree. So this checks the invariant
instead — every path that repositions a stuck ball goes through
`rest_ball_on_bat`.

`respawn_primary_ball` is the deliberate exception: it assigns
`stuck_offset_x = BALL_X_OFFSET_ON_BAT` on the line above, so the two
are equal by construction there.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


def body_of(src: str, signature: str) -> str:
    start = src.index(signature)
    depth = 0
    i = src.index("{", start)
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise SystemExit(f"FAIL: could not find the end of {signature}")


def main() -> int:
    src = SRC.read_text()

    if "static void rest_ball_on_bat(int b, int bat)" not in src:
        raise SystemExit("FAIL: rest_ball_on_bat is gone; update this gate")

    body = body_of(src, "static void redraw_frame(")
    if "ball.stuck" not in body:
        raise SystemExit("FAIL: redraw_frame no longer repositions a stuck ball; "
                         "update this gate")
    if "rest_ball_on_bat(BALL_PRIMARY, ball.stuck_bat[BALL_PRIMARY]);" not in body:
        raise SystemExit(
            "FAIL: redraw_frame places a stuck ball without rest_ball_on_bat — "
            "known-bugs #12. It must not hardcode an offset; the catch offset "
            "lives in ball.stuck_offset_x and the launch direction derives "
            "from it.")
    if "BALL_X_OFFSET_ON_BAT" in body:
        raise SystemExit(
            "FAIL: redraw_frame still mentions BALL_X_OFFSET_ON_BAT; the "
            "stuck-ball position must come from ball.stuck_offset_x")
    print("PASS stuck_ball_offset: redraw_frame defers to rest_ball_on_bat")

    # rest_ball_on_bat itself must keep using the recorded offset.
    rest = body_of(src, "static void rest_ball_on_bat(int b, int bat)")
    if "ball.stuck_offset_x" not in rest:
        raise SystemExit("FAIL: rest_ball_on_bat no longer uses the recorded "
                         "catch offset")
    print("PASS stuck_ball_offset_source: rest_ball_on_bat reads stuck_offset_x")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
