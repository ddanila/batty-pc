#!/usr/bin/env python3
"""Only the primary ball may write the primary ball's sign cache.

known-bugs.md #13. `ball.dx` / `ball.dy` reduce the PRIMARY ball's
direction to signs (-1/0/+1). Two things read them:

  - `ball_lands_on_bat()`      gates the bat hit on `ball.dy > 0`
  - `apply_multi_ball_bonus()` derives the extras' launch directions

But two of the functions that refresh them take an `Object *` that may be
an EXTRA ball — `laffc_collision` (reached from `step_extra_ball`) and
`magnet_ball_frame` (slots 1 and 2) — and used to write the primary's
cache from whichever ball they were handed.

No pixel gate reaches the scenario that makes that observable: extras
out, one bounces off a brick, the primary STUCK on the bat so `step_ball`
returns before refreshing, the extras die, then MULTIBALL is caught. That
is four coincident conditions, and `test-magnet-ball` plus the multiball
gates reach none of them together. So the guard is structural instead:
the refresh takes the object it is refreshing FROM and ignores anything
that is not the primary, which makes the bug unexpressible at a call
site rather than merely absent from today's call sites.

This gate fails against the code as it stood at d7daa83, which is the
evidence that it guards the thing it claims to.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"

HELPER = "refresh_ball_motion_signs"


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
    bad = []

    # 1. the cache is written in exactly one place
    writes = len(re.findall(r"ball\.dx\s*=\s*\(", src)) + \
             len(re.findall(r"ball\.dy\s*=\s*\(", src))
    if writes != 2:
        bad.append(f"expected the sign cache to be written in one place "
                   f"(one ball.dx and one ball.dy assignment, inside "
                   f"{HELPER}); found {writes} such assignments")

    # 2. that place takes the object it refreshes FROM
    m = re.search(r"static void " + HELPER + r"\(([^)]*)\)", src)
    if not m:
        bad.append(f"{HELPER} is gone; if the cache moved, update this gate")
    else:
        params = m.group(1)
        if "Object" not in params:
            bad.append(
                f"{HELPER} does not take the Object it refreshes from, so a "
                f"caller holding an EXTRA ball can still write the primary's "
                f"cache — known-bugs #13. Signature is: ({params.strip()})")
        else:
            body = body_of(src, "static void " + HELPER + "(")
            if "OBJ_BALL_1" not in body:
                bad.append(
                    f"{HELPER} takes an Object but never checks it against "
                    f"OBJ_BALL_1, so extras still write the primary's cache")

    # 3. the two callers that can hold an extra ball must pass their own
    #    object through, not the primary
    for fn in ("static int laffc_collision(", "static int magnet_ball_frame("):
        try:
            body = body_of(src, fn)
        except (ValueError, SystemExit):
            continue
        for call in re.findall(HELPER + r"\(([^,)]*)", body):
            arg = call.strip()
            if "OBJ_BALL_1" in arg:
                bad.append(
                    f"{fn.split('(')[0].split()[-1]} passes the PRIMARY to "
                    f"{HELPER} even though it can be called with an extra "
                    f"ball; it must pass its own object")

    if bad:
        print("FAIL: the ball sign cache does not have a single owner\n")
        for b in bad:
            print(f"  - {b}")
        print("\nSee notes/known-bugs.md #13. ball.dx/ball.dy belong to the "
              "primary ball; an extra ball must not be able to write them.")
        return 1

    print(f"PASS ball_sign_cache_owner: only the primary writes ball.dx/dy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
